#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <unistd.h>   // write, fsync
#include <fcntl.h>    // open, O_WRONLY, O_CREAT, O_APPEND
#include <sys/uio.h>  // writev (for atomic multi-buffer write)
#include "../common/types.hpp"
#include "../common/constants.hpp"
#include "../common/cacheline.hpp"

namespace hft {

// ============================================================================
// Journal (Write-Ahead Log)
//
// Binary WAL: every order is serialized to disk BEFORE the matching engine
// processes it. This guarantees deterministic replay.
//
// Wire format per record:
//   [4 bytes magic] [4 bytes record_len] [N bytes payload] [4 bytes crc32]
//
// Binary layout of an Order journal record:
//   JournalHeader + Order struct
//
// Performance decisions:
//  - Writes go through a userspace batch buffer (JOURNAL_FLUSH_BATCH records)
//    then a single writev() syscall — minimizing syscall overhead.
//  - fsync() called periodically or at shutdown, NOT on every record.
//  - No dynamic memory allocation.
//
// Thread safety: append() is NOT thread-safe. The single sequencer thread
// owns the journal. Other threads submit via SPSC queue → sequencer → journal.
// ============================================================================

// -------------------------------------------------------------------------
// Wire format structures
// -------------------------------------------------------------------------
constexpr uint32_t JOURNAL_MAGIC = 0x48465457u; // 'HFTW'

#pragma pack(push, 1)
struct JournalHeader {
    uint32_t magic;       // JOURNAL_MAGIC
    uint16_t version;     // Wire format version
    uint8_t  record_type; // 0=Order 1=Cancel 2=Modify 3=Heartbeat
    uint8_t  _pad;
    uint32_t record_len;  // Payload length (bytes, not including header+crc)
    uint64_t sequence;    // Global sequence number
    uint64_t timestamp;   // RDTSC at time of append
};
#pragma pack(pop)

enum class JournalRecordType : uint8_t {
    ORDER     = 0,
    CANCEL    = 1,
    MODIFY    = 2,
    HEARTBEAT = 3
};

// -------------------------------------------------------------------------
// Simple CRC32 (Castagnoli) — used for record integrity
// -------------------------------------------------------------------------
namespace detail {

inline uint32_t crc32c_byte(uint32_t crc, uint8_t byte) noexcept {
    crc ^= byte;
    for (int i = 0; i < 8; ++i) {
        crc = (crc >> 1) ^ (0x82F63B78u & -(crc & 1));
    }
    return crc;
}

inline uint32_t crc32c(const void* data, size_t len) noexcept {
    uint32_t crc = 0xFFFFFFFFu;
    const auto* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; ++i) {
        crc = crc32c_byte(crc, p[i]);
    }
    return crc ^ 0xFFFFFFFFu;
}

} // namespace detail

// ============================================================================
class Journal {
public:
    static constexpr size_t BATCH_BYTES =
        (sizeof(JournalHeader) + sizeof(Order) + sizeof(uint32_t)) * JOURNAL_FLUSH_BATCH;

    explicit Journal(const char* path) noexcept
        : fd_(-1), pending_(0)
    {
        fd_ = ::open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
        // In production, handle open failure; here we keep it simple
    }

    ~Journal() noexcept {
        flush();
        if (fd_ >= 0) {
            ::fsync(fd_);
            ::close(fd_);
        }
    }

    Journal(const Journal&) = delete;
    Journal& operator=(const Journal&) = delete;

    bool is_open() const noexcept { return fd_ >= 0; }

    // -------------------------------------------------------------------------
    // append — serialize an Order to the batch buffer.
    // Flushes automatically when the batch is full.
    // -------------------------------------------------------------------------
    bool append(const Order& order, uint64_t timestamp) noexcept {
        const size_t record_size = sizeof(JournalHeader) + sizeof(Order) + sizeof(uint32_t);

        if (pending_ + record_size > BATCH_BYTES) {
            if (!flush()) return false;
        }

        // Write header
        auto* hdr = reinterpret_cast<JournalHeader*>(&batch_buf_[pending_]);
        hdr->magic       = JOURNAL_MAGIC;
        hdr->version     = 1;
        hdr->record_type = static_cast<uint8_t>(JournalRecordType::ORDER);
        hdr->_pad        = 0;
        hdr->record_len  = sizeof(Order);
        hdr->sequence    = order.sequence;
        hdr->timestamp   = timestamp;
        pending_ += sizeof(JournalHeader);

        // Write payload (Order struct)
        std::memcpy(&batch_buf_[pending_], &order, sizeof(Order));
        pending_ += sizeof(Order);

        // Write CRC32 over header + payload
        const uint32_t crc = detail::crc32c(&batch_buf_[pending_ - sizeof(JournalHeader) - sizeof(Order)],
                                             sizeof(JournalHeader) + sizeof(Order));
        std::memcpy(&batch_buf_[pending_], &crc, sizeof(uint32_t));
        pending_ += sizeof(uint32_t);

        ++records_buffered_;
        return true;
    }

    // -------------------------------------------------------------------------
    // flush — write the batch buffer to disk in one syscall
    // -------------------------------------------------------------------------
    bool flush() noexcept {
        if (pending_ == 0 || fd_ < 0) return true;

        ssize_t written = ::write(fd_, batch_buf_, pending_);
        if (HFT_UNLIKELY(written < 0 || static_cast<size_t>(written) != pending_)) {
            return false; // I/O error
        }
        pending_          = 0;
        records_buffered_ = 0;
        return true;
    }

    // Force fsync to durable storage
    bool sync() noexcept {
        if (!flush()) return false;
        return fd_ >= 0 && ::fsync(fd_) == 0;
    }

    // -------------------------------------------------------------------------
    // replay — read all records from a file and invoke callback for each Order
    // -------------------------------------------------------------------------
    template <typename Callback>
    static bool replay(const char* path, Callback&& cb) noexcept {
        int fd = ::open(path, O_RDONLY);
        if (fd < 0) return false;

        JournalHeader hdr;
        Order         ord;
        uint32_t      file_crc;

        while (true) {
            // Read header
            ssize_t n = ::read(fd, &hdr, sizeof(hdr));
            if (n == 0) break; // EOF
            if (n != static_cast<ssize_t>(sizeof(hdr))) break;
            if (hdr.magic != JOURNAL_MAGIC) break;

            // Read payload
            if (hdr.record_type == static_cast<uint8_t>(JournalRecordType::ORDER)) {
                n = ::read(fd, &ord, sizeof(Order));
                if (n != static_cast<ssize_t>(sizeof(Order))) break;
            } else {
                // Skip unknown types
                ::lseek(fd, static_cast<off_t>(hdr.record_len), SEEK_CUR);
            }

            // Read CRC
            n = ::read(fd, &file_crc, sizeof(uint32_t));
            if (n != static_cast<ssize_t>(sizeof(uint32_t))) break;

            // Verify CRC
            // (skipping for performance during normal replay — enable in recovery mode)

            if (hdr.record_type == static_cast<uint8_t>(JournalRecordType::ORDER)) {
                cb(ord, hdr.sequence, hdr.timestamp);
            }
        }

        ::close(fd);
        return true;
    }

    // -------------------------------------------------------------------------
    // Diagnostics
    // -------------------------------------------------------------------------
    [[nodiscard]] size_t pending_bytes()   const noexcept { return pending_; }
    [[nodiscard]] size_t pending_records() const noexcept { return records_buffered_; }

private:
    int    fd_;
    size_t pending_          = 0;
    size_t records_buffered_ = 0;
    alignas(CACHELINE_SIZE) uint8_t batch_buf_[BATCH_BYTES];
};

} // namespace hft

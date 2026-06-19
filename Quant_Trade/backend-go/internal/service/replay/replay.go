package replay

import (
	"encoding/binary"
	"fmt"
	"io"
	"os"
)

type JournalHeader struct {
	Magic      uint32
	Version    uint16
	RecordType uint8
	Pad        uint8
	RecordLen  uint32
	Sequence   uint64
	Timestamp  uint64
}

type ReplayRecord struct {
	Header JournalHeader
	Data   []byte
	CRC    uint32
}

type Service struct{}

func New() *Service {
	return &Service{}
}

func (s *Service) ReplayJournal(path string, callback func(record *ReplayRecord) error) error {
	file, err := os.Open(path)
	if err != nil {
		return err
	}
	defer file.Close()

	for {
		var hdr JournalHeader
		err = binary.Read(file, binary.LittleEndian, &hdr)
		if err == io.EOF {
			break
		}
		if err != nil {
			return err
		}

		if hdr.Magic != 0x48465457 {
			return fmt.Errorf("invalid journal magic number: %x", hdr.Magic)
		}

		data := make([]byte, hdr.RecordLen)
		_, err = io.ReadFull(file, data)
		if err != nil {
			return err
		}

		var crc uint32
		err = binary.Read(file, binary.LittleEndian, &crc)
		if err != nil {
			return err
		}

		record := &ReplayRecord{
			Header: hdr,
			Data:   data,
			CRC:    crc,
		}

		if err := callback(record); err != nil {
			return err
		}
	}

	return nil
}

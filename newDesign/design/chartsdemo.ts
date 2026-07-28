import * as React from "react";
import { ActivityChartCard } from "@/components/ui/activity-chart-card";

// Sample data for the demo
const weeklyActivityData = [
  { day: "S", value: 8 },
  { day: "M", value: 12 },
  { day: "T", value: 9 },
  { day: "W", value: 4 },
  { day: "T", value: 7 },
  { day: "F", value: 14 },
  { day: "S", value: 2 },
];

/**
 * A demo component to showcase the ActivityChartCard.
 */
export default function ActivityChartCardDemo() {
  return (
    <div className="flex min-h-[400px] w-full items-center justify-center bg-background p-4">
      <ActivityChartCard
        title="Activity"
        totalValue="21h"
        data={weeklyActivityData}
      />
    </div>
  );
}
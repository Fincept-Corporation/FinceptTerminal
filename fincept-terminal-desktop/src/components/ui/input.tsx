import * as React from "react"

import { cn } from "@/lib/utils"

function Input({ className, type, ...props }: React.ComponentProps<"input">) {
  return (
    <input
      type={type}
      data-slot="input"
      className={cn(
        "h-8 w-full min-w-0 border bg-[#0d0d0d] border-[#333] px-3 py-1 text-xs text-[#e0e0e0] font-mono",
        "placeholder:text-[#666] selection:bg-orange-500/30 selection:text-white",
        "transition-colors outline-none",
        "focus:border-[#FF6600] focus:ring-0",
        "disabled:pointer-events-none disabled:cursor-not-allowed disabled:opacity-50",
        className
      )}
      {...props}
    />
  )
}

export { Input }

#!/usr/bin/env python3
import sys
import re
from typing import Optional, Dict, Any, Tuple

DEFAULT_S = 5
DEFAULT_E = 1
DEFAULT_B = 5

def print_help():
    help_text = """
Usage:
  python3 parse_trace.py <input_trace> <output_file> [s] [E] [b]
  python3 parse_trace.py -h | --help

Description:
  解析 CS:APP Cache Lab 格式的内存轨迹文件（trace），输出结构化的
  Cache 映射详情（Tag, Set Index, Block Offset），并模拟缓存访问
  状态（hit, miss, miss eviction）。
  当发生 eviction 时，会打印出被替换块的基地址（起始地址）。
"""
    print(help_text.strip())

class CacheSimulator:
    """带被替换地址追溯的 Cache 模拟器"""
    def __init__(self, s: int, E: int, b: int):
        self.s = s
        self.E = E
        self.b = b
        self.num_sets = 1 << s
        self.cache = [[{"valid": False, "tag": 0, "lru": 0} for _ in range(E)] for _ in range(self.num_sets)]
        self.time_counter = 0

    def access(self, tag: int, set_idx: int) -> Tuple[str, Optional[int]]:
        """
        返回: (status_str, evicted_block_addr)
        若没有发生替换，evicted_block_addr 为 None
        """
        self.time_counter += 1
        lines = self.cache[set_idx]

        # 1. 查找 Hit
        for line in lines:
            if line["valid"] and line["tag"] == tag:
                line["lru"] = self.time_counter
                return "hit", None

        # 2. 查找是否有空位 (Cold Miss)
        for line in lines:
            if not line["valid"]:
                line["valid"] = True
                line["tag"] = tag
                line["lru"] = self.time_counter
                return "miss", None

        # 3. 组满替换 (Miss + Eviction)
        lru_idx = min(range(self.E), key=lambda idx: lines[idx]["lru"])
        
        # 计算被逐出块的起始内存基地址
        old_tag = lines[lru_idx]["tag"]
        evicted_addr = (old_tag << (self.s + self.b)) | (set_idx << self.b)

        # 更新为新行
        lines[lru_idx]["tag"] = tag
        lines[lru_idx]["lru"] = self.time_counter
        return "miss eviction", evicted_addr


def parse_trace_line(line: str, s: int, b: int) -> Optional[Dict[str, Any]]:
    match = re.search(r"([LSM])\s+([0-9a-fA-F]+),(\d+)", line)
    if not match:
        return None

    op = match.group(1)
    addr = int(match.group(2), 16)
    size = int(match.group(3))

    block_offset = addr & ((1 << b) - 1)
    set_index = (addr >> b) & ((1 << s) - 1)
    tag = addr >> (s + b)

    total_low_bits = s + b
    low_binary = f"{(addr & ((1 << total_low_bits) - 1)):0{total_low_bits}b}"
    set_bits = low_binary[:s]
    offset_bits = low_binary[s:]

    return {
        "op": op,
        "addr": addr,
        "size": size,
        "tag": tag,
        "set_index": set_index,
        "block_offset": block_offset,
        "set_bits": set_bits,
        "offset_bits": offset_bits,
    }


def process_file(input_filepath: str, output_filepath: str, s: int = DEFAULT_S, E: int = DEFAULT_E, b: int = DEFAULT_B):
    sim = CacheSimulator(s, E, b)
    
    # 增加一列: Evicted Addr
    header = (
        f"{'Raw Trace':<18} | {'Op':<2} | {'Address':<10} | {'Tag (Hex)':<10} | "
        f"{'Set':<5} | {'Offset':<6} | {'Set_Bits Offset_Bits':<20} | {'Status':<15} | {'Evicted Block Base'}\n"
    )
    separator = "-" * 122 + "\n"

    total_hits = 0
    total_misses = 0
    total_evictions = 0

    with open(input_filepath, "r", encoding="utf-8") as fin, open(output_filepath, "w", encoding="utf-8") as fout:
        fout.write(f"# Cache Configuration: s={s} (Sets={1<<s}), E={E}, b={b} (BlockSize={1<<b}B)\n")
        fout.write(header)
        fout.write(separator)

        for line in fin:
            line_str = line.strip()
            if not line_str or line_str.startswith("I"):
                continue

            parsed = parse_trace_line(line_str, s, b)
            if not parsed:
                continue

            op = parsed["op"]
            tag = parsed["tag"]
            set_idx = parsed["set_index"]

            access_times = 2 if op == "M" else 1
            statuses = []
            evicted_addrs = []

            for _ in range(access_times):
                status, evicted_addr = sim.access(tag, set_idx)
                statuses.append(status)
                if evicted_addr is not None:
                    evicted_addrs.append(f"0x{evicted_addr:08x}")
                if "hit" in status:
                    total_hits += 1
                if "miss" in status:
                    total_misses += 1
                if "eviction" in status:
                    total_evictions += 1

            status_str = " ".join(statuses)
            evicted_str = ", ".join(evicted_addrs) if evicted_addrs else "-"
            binary_split = f"{parsed['set_bits']} {parsed['offset_bits']}"

            fout.write(
                f"{line_str:<18} | {op:<2} | 0x{parsed['addr']:08x} | 0x{tag:<8x} | "
                f"{set_idx:<5} | {parsed['block_offset']:<6} | {binary_split:<20} | "
                f"{status_str:<15} | {evicted_str}\n"
            )

        fout.write(separator)
        fout.write(f"# Summary: hits:{total_hits} misses:{total_misses} evictions:{total_evictions}\n")

    print(f"[+] 解析完成！结果已写入: {output_filepath}")
    print(f"    统计结果: hits={total_hits}, misses={total_misses}, evictions={total_evictions}")


if __name__ == "__main__":
    if len(sys.argv) == 2 and sys.argv[1] in ("-h", "--help"):
        print_help()
        sys.exit(0)
        
    if len(sys.argv) < 3:
        print("Usage: python3 parse_trace.py <input_trace_file> <output_result_file> [s] [E] [b]")
        sys.exit(1)

    in_file = sys.argv[1]
    out_file = sys.argv[2]
    s_val = int(sys.argv[3]) if len(sys.argv) > 3 else DEFAULT_S
    E_val = int(sys.argv[4]) if len(sys.argv) > 4 else DEFAULT_E
    b_val = int(sys.argv[5]) if len(sys.argv) > 5 else DEFAULT_B

    process_file(in_file, out_file, s_val, E_val, b_val)
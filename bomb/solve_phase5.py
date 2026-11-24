#!/usr/bin/env python3
import string

# 1. Phase 5 的 16 字节查找表（截取前 16 位，因为 & 0xf 的范围是 0~15）
RAW_TABLE = "aduiersnfotvbylSo you think you can stop the bomb with ctrl-c, do you"
TABLE = RAW_TABLE[:15]

# 2. 你的 Phase 5 要求的目标字符串（请替换为你在 strings/GDB 中查到的目标，如 "flyers", "devils", "giants" 等）
TARGET = "flyers"


def solve(target, table):
    print(f"=== 查找表 (长度 {len(table)}) ===")
    for idx, ch in enumerate(table):
        print(f"  下标 0x{idx:x} ({idx:2d}) -> '{ch}'")
    print("=" * 30)

    result_easy = []
    print(f"\n目标字符串: \"{target}\"\n")
    print(f"{'目标字符':<8} {'需要下标(低4位)':<16} {'候选可见 ASCII 字符'}")
    print("-" * 55)

    for ch in target:
        if ch not in table:
            print(f"[错误] 目标字符 '{ch}' 不在查找表中，请检查输入或地址！")
            return

        needed_idx = table.index(ch)

        # 找出所有 (ASCII & 0xf == needed_idx) 的常见可见字符
        candidates = [
            c
            for c in string.printable
            if ord(c) & 0xF == needed_idx and c not in string.whitespace
        ]

        # 优先挑一个小写字母作为标准推荐，没有小写字母就取候选里的第 1 个
        lower_cand = [c for c in candidates if c.islower()]
        chosen = lower_cand[0] if lower_cand else candidates[0]
        result_easy.append(chosen)

        cand_str = " ".join(
            f"'{c}'(0x{ord(c):02x})" for c in candidates[:6]
        )  # 展示部分候选
        print(f"'{ch}'      0x{needed_idx:x} ({needed_idx:2d})          {cand_str}")

    print("-" * 55)
    final_str = "".join(result_easy)
    print(f"\n[推荐答案 (纯小写)]: {final_str}")


if __name__ == "__main__":
    solve(TARGET, TABLE)
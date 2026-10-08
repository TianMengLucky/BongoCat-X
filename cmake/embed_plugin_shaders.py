"""Embed generated shader binaries into the optional Cubism plugin only."""
import argparse
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True)
    parser.add_argument("--list", required=True)
    args = parser.parse_args()
    rows = ["#include <cstdlib>", "#include <cstring>", "#include <string>"]
    entries = []
    for index, filename in enumerate(Path(args.list).read_text(encoding="utf-8").splitlines()):
        path = Path(filename)
        if not filename:
            continue
        data = path.read_bytes()
        rows.append(f"static const unsigned char shader_{index}[] = {{")
        for start in range(0, len(data), 24):
            rows.append(",".join(str(b) for b in data[start:start + 24]) + ",")
        rows.append("};")
        entries.append(f'{{"{path.name}", shader_{index}, sizeof(shader_{index})}}')
    rows.append("struct Entry { const char *name; const unsigned char *bytes; size_t size; };")
    rows.append("static const Entry entries[] = {" + ",".join(entries or ["{nullptr,nullptr,0}"]) + "};")
    rows.append("""
unsigned char *bongo_cat_cubism_shader_bytes(const std::string& path, size_t *size) {
    const auto slash = path.find_last_of("/\\\\");
    const char *name = path.c_str() + (slash == std::string::npos ? 0 : slash + 1);
    for (const auto& entry : entries) {
        if (!entry.name || std::strcmp(name, entry.name) != 0) continue;
        auto *bytes = static_cast<unsigned char *>(std::malloc(entry.size));
        if (!bytes) return nullptr;
        std::memcpy(bytes, entry.bytes, entry.size);
        if (size) *size = entry.size;
        return bytes;
    }
    return nullptr;
}
""")
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(rows), encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()

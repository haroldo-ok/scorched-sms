import re, sys
# fail the build if fixed code reaches the SDSC header (0x7FE0) or bank 2 overflows
m = open(sys.argv[1]).read()
areas = {}
for a in re.finditer(r'^(_\w+)\s+([0-9A-F]{8})\s+([0-9A-F]{8})\s*=', m, re.M):
    areas[a.group(1)] = (int(a.group(2), 16), int(a.group(3), 16))
end_fixed = max(s + l for n, (s, l) in areas.items() if s < 0x8000 and n not in ('_HEADER', '_HEADER0', '_HEADER1', '_HEADER2'))
b2 = areas.get('_BANK2', (0x8000, 0))
dat = max(s + l for n, (s, l) in areas.items() if s >= 0xC000)
print("fixed ROM ends 0x%04X (limit 0x7FE0), bank2 0x%04X..0x%04X, RAM ends 0x%04X" % (end_fixed, b2[0], b2[0] + b2[1], dat))
for n,(s,l) in sorted(areas.items(), key=lambda x:x[1]): print("   %-12s %04X %5d" % (n, s, l))
if end_fixed > 0x7FE0 or b2[0] + b2[1] > 0xC000 or dat > 0xDE00:
    sys.exit("LAYOUT OVERFLOW")

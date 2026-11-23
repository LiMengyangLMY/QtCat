import struct
import os

mobi_path = os.path.join("docs", "sample_novel.mobi")

text_content = """<html><body>
<h1>第三章 灵界传闻 (MOBI 体验篇)</h1>
<p>微风吹过青牛村后山古老而幽静的竹林，竹影婆娑。</p>
<p>少年握着手中灵石，感受着其中温润平和的灵气流转，心境前所未有地清澈宁静。</p>
<p>小狸猫趴在少年的肩膀上，懒洋洋地打了个哈欠，尾巴轻轻晃了晃，享受着悠闲静谧的午后时光……</p>
</body></html>""".encode("utf-8")

# Record 0: PalmDOC Header (16 bytes) + MOBI Header
compression = 1  # uncompressed
text_length = len(text_content)
text_record_count = 1
record_size = 4096
encryption = 0

palmdoc_hdr = struct.pack(">HHIHHII", compression, 0, text_length, text_record_count, record_size, encryption, 0)

# MOBI Header
mobi_magic = b"MOBI"
mobi_hdr_len = 232
mobi_type = 2
text_encoding = 65001  # UTF-8
mobi_hdr = struct.pack(">4sIII", mobi_magic, mobi_hdr_len, mobi_type, text_encoding)
mobi_hdr += b"\x00" * (mobi_hdr_len - len(mobi_hdr))

record0_data = palmdoc_hdr + mobi_hdr

num_records = 2
header_size = 78 + (num_records * 8) + 2  # 78 + 16 + 2 padding = 96

rec0_offset = header_size
rec1_offset = rec0_offset + len(record0_data)

# PDB Header (78 bytes)
pdb_name = b"SampleMobi" + b"\x00" * 22
pdb_attrs = 0
pdb_version = 0
timestamps = b"\x00" * 16
app_id = 0
sort_id = 0
type_id = b"BOOK"
creator_id = b"MOBI"
seed = 0
next_rec = 0

pdb_header = struct.pack(
    ">32sHH16sII4s4sIIH",
    pdb_name,
    pdb_attrs,
    pdb_version,
    timestamps,
    app_id,
    sort_id,
    type_id,
    creator_id,
    seed,
    next_rec,
    num_records,
)

# Record entries (8 bytes each)
rec0_entry = struct.pack(">II", rec0_offset, 0)
rec1_entry = struct.pack(">II", rec1_offset, 2)
padding = b"\x00\x00"

with open(mobi_path, "wb") as f:
    f.write(pdb_header)
    f.write(rec0_entry)
    f.write(rec1_entry)
    f.write(padding)
    f.write(record0_data)
    f.write(text_content)

print(f"Generated {mobi_path}, size: {os.path.getsize(mobi_path)} bytes")

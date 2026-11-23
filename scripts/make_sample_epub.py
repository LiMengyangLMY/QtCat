import zipfile
import os

epub_path = os.path.join("docs", "sample_novel.epub")

with zipfile.ZipFile(epub_path, "w", zipfile.ZIP_DEFLATED) as z:
    # 1. mimetype (uncompressed)
    z.writestr("mimetype", b"application/epub+zip", compress_type=zipfile.ZIP_STORED)

    # 2. META-INF/container.xml
    container = """<?xml version="1.0"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles>
    <rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>
  </rootfiles>
</container>"""
    z.writestr("META-INF/container.xml", container.encode("utf-8"))

    # 3. OEBPS/content.opf
    opf = """<?xml version="1.0" encoding="utf-8"?>
<package xmlns="http://www.idpf.org/2007/opf" unique-identifier="BookId" version="2.0">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
    <dc:title>修仙小狸奴 (EPUB 体验本)</dc:title>
    <dc:language>zh-CN</dc:language>
  </metadata>
  <manifest>
    <item id="ch1" href="ch1.xhtml" media-type="application/xhtml+xml"/>
    <item id="ch2" href="ch2.xhtml" media-type="application/xhtml+xml"/>
  </manifest>
  <spine>
    <itemref idref="ch1"/>
    <itemref idref="ch2"/>
  </spine>
</package>"""
    z.writestr("OEBPS/content.opf", opf.encode("utf-8"))

    # 4. Chapters
    ch1 = """<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN" "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head><title>第一章 青牛山初遇</title></head>
<body>
<h1>第一章 青牛山初遇</h1>
<p>山风徐徐，清晨的青牛村笼罩在一层薄薄的晨雾之中。</p>
<p>少年背着破竹剑走在林荫山道上，四周松涛阵阵，鸟鸣清脆。</p>
<p>“只要今日采到宁神草，师妹的病就有救了。”他暗下决心。</p>
</body></html>"""
    z.writestr("OEBPS/ch1.xhtml", ch1.encode("utf-8"))

    ch2 = """<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN" "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head><title>第二章 狸猫异变</title></head>
<body>
<h1>第二章 狸猫异变</h1>
<p>悬崖峭壁之上，一抹翠绿荧光随风微晃，正是百年难遇的宁神草！</p>
<p>忽然，灌木丛中探出一颗毛茸茸的小圆脑袋，竟是一只体态微胖的小花猫。</p>
<p>小猫双目宛如琉璃琥珀，冲着少年轻轻喵了一声，一道淡淡的金色灵纹从它额头浮现……</p>
</body></html>"""
    z.writestr("OEBPS/ch2.xhtml", ch2.encode("utf-8"))

print(f"Generated {epub_path}, size: {os.path.getsize(epub_path)} bytes")

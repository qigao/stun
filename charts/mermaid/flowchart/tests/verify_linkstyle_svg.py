"""Inspect real production-renderer SVG, independently of Mustache string tests."""
import sys
import xml.etree.ElementTree as ET

root = ET.parse(sys.argv[1]).getroot()
ns = {"s": "http://www.w3.org/2000/svg"}
paths = [p for p in root.findall(".//s:path", ns) if p.get("class") == "edge-path"]
assert [p.get("data-edge-index") for p in paths] == ["0", "1", "2"]
assert [(p.get("stroke"), p.get("stroke-width")) for p in paths] == [
    ("#123456", "7"), ("greenyellow", "2"), ("#123456", "7")]
for i, path in enumerate(paths):
    marker_id = f"arrowhead-{i}"
    assert path.get("marker-end") == f"url(#{marker_id})"
    markers = root.findall(f'.//s:marker[@id="{marker_id}"]', ns)
    assert len(markers) == 1
    assert markers[0].find("s:path", ns).get("fill") == path.get("stroke")
print("Independent SVG XML: second declared edge and its arrowhead retain greenyellow/2px")

"""Read source surface colors for static FBX material slots."""
import bpy,json,sys
from pathlib import Path
out=Path(sys.argv[-1])
result={}
for m in bpy.data.materials:
    node=next((n for n in m.node_tree.nodes if n.type=='BSDF_PRINCIPLED'),None) if m.node_tree else None
    color=list(node.inputs["Base Color"].default_value) if node else list(m.diffuse_color)
    result[m.name]=[color, node.inputs["Metallic"].default_value if node else m.metallic,
                    node.inputs["Roughness"].default_value if node else m.roughness]
out.write_text(json.dumps(result,ensure_ascii=False,indent=1),encoding="utf8")


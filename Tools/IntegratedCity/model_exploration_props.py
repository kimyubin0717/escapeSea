"""Build four editable, low-poly loot props through the live Blender MCP."""
import json
import math
from pathlib import Path

import bmesh
import bpy
from mathutils import Matrix, Vector

root = Path.home() / 'OneDrive' / '\ubb38\uc11c' / 'Unreal Projects/mama'
output = root / 'Source/SourceArt/IntegratedCity/ExplorationProps'
output.mkdir(parents=True, exist_ok=True)
palette = {
    'M_ExplorerSteel': ((0.045, 0.16, 0.18, 1), 0.0, 0.55),
    'M_ExplorerBrass': ((0.65, 0.4, 0.14, 1), 1.0, 0.38),
    'M_ExplorerWood': ((0.24, 0.11, 0.045, 1), 0.0, 0.74),
}
materials = {}
for name, (color, metallic, roughness) in palette.items():
    material = bpy.data.materials.new(name)
    material.diffuse_color = color
    material.use_nodes = True
    shader = next(n for n in material.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    shader.inputs['Base Color'].default_value = color
    shader.inputs['Metallic'].default_value = metallic
    shader.inputs['Roughness'].default_value = roughness
    materials[name] = material


class Assembly:
    def __init__(self):
        self.vertices = []
        self.faces = []
        self.slots = []

    def add(self, vertices, faces, material=0, transform=None):
        offset = len(self.vertices)
        for vertex in vertices:
            value = transform @ Vector(vertex) if transform else Vector(vertex)
            self.vertices.append(tuple(value))
        self.faces.extend(tuple(offset + index for index in face) for face in faces)
        self.slots.extend([material] * len(faces))

    def box(self, center, dimensions, material=0, transform=None):
        x, y, z = (v / 2 for v in dimensions)
        vertices = [(a*x+center[0], b*y+center[1], c*z+center[2])
                    for a,b,c in ((-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),
                                  (-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1))]
        self.add(vertices, [(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)], material, transform)

    def cylinder(self, center, radius, depth, material=1, axis='Y', segments=12, transform=None):
        vertices = []
        for end in (-1, 1):
            for i in range(segments):
                a = i * 2 * math.pi / segments
                v = (radius*math.cos(a), end*depth/2, radius*math.sin(a))
                if axis == 'Z':
                    v = (v[0], v[2], v[1])
                vertices.append(tuple(center[j] + v[j] for j in range(3)))
        faces = [tuple(range(segments-1,-1,-1)), tuple(range(segments,2*segments))]
        faces += [(i,(i+1)%segments,(i+1)%segments+segments,i+segments) for i in range(segments)]
        self.add(vertices, faces, material, transform)

    def hollow_box(self, width, depth, height, thickness, material=0):
        vertices = []
        for z, w, d in ((0,width,depth),(height,width,depth),(thickness,width-2*thickness,depth-2*thickness),(height,width-2*thickness,depth-2*thickness)):
            vertices += [(-w/2,-d/2,z),(w/2,-d/2,z),(w/2,d/2,z),(-w/2,d/2,z)]
        faces = [(0,3,2,1),(8,9,10,11)]
        for i in range(4):
            j=(i+1)%4
            faces += [(i,j,j+4,i+4),(i+8,i+12,j+12,j+8),(i+4,j+4,j+12,i+12)]
        self.add(vertices, faces, material)

    def curved_lid(self, width, depth, height, base, thickness, material=0, transform=None, x_offset=0):
        vertices = []
        count = 13
        for x, inner in ((-width/2,False),(width/2,False),(-width/2,True),(width/2,True)):
            for i in range(count):
                a = math.pi*i/(count-1)
                r = depth/2 - (thickness if inner else 0)
                h = height - (thickness if inner else 0)
                vertices.append((x+x_offset, r*math.cos(a), base+h*math.sin(a)))
        faces = []
        for i in range(count-1):
            faces += [(i,i+1,count+i+1,count+i),
                      (2*count+i,3*count+i,3*count+i+1,2*count+i+1),
                      (i,2*count+i,2*count+i+1,i+1),
                      (count+i,count+i+1,3*count+i+1,3*count+i)]
        faces += [(0,count,3*count,2*count),(count-1,3*count-1,4*count-1,2*count-1)]
        self.add(vertices, faces, material, transform)

    def make(self, name, slots):
        mesh = bpy.data.meshes.new(name)
        mesh.from_pydata(self.vertices, [], self.faces)
        for material in slots:
            mesh.materials.append(materials[material])
        for face, slot in zip(mesh.polygons, self.slots):
            face.material_index = slot
        bm = bmesh.new()
        bm.from_mesh(mesh)
        bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
        bm.to_mesh(mesh)
        bm.free()
        scene = bpy.data.scenes.new(name)
        obj = bpy.data.objects.new(name, mesh)
        scene.collection.objects.link(obj)
        bpy.context.window.scene = scene
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bevel = obj.modifiers.new('Small manufactured edge bevels', 'BEVEL')
        bevel.width = 0.006
        bevel.segments = 1
        bevel.limit_method = 'ANGLE'
        with bpy.context.temp_override(object=obj, active_object=obj, selected_objects=[obj], selected_editable_objects=[obj]):
            assert bpy.ops.object.modifier_apply(modifier=bevel.name) == {'FINISHED'}
            cleaned = bmesh.new()
            cleaned.from_mesh(mesh)
            bmesh.ops.remove_doubles(cleaned, verts=list(cleaned.verts), dist=.00001)
            bmesh.ops.dissolve_degenerate(cleaned, dist=.00001, edges=list(cleaned.edges))
            tiny = [face for face in cleaned.faces if face.calc_area() < 1e-10]
            if tiny:
                bmesh.ops.delete(cleaned, geom=tiny, context='FACES_ONLY')
            bmesh.ops.triangulate(cleaned, faces=list(cleaned.faces))
            bmesh.ops.recalc_face_normals(cleaned, faces=list(cleaned.faces))
            cleaned.to_mesh(mesh)
            cleaned.free()
            assert bpy.ops.object.mode_set(mode='EDIT') == {'FINISHED'}
            bpy.ops.mesh.select_all(action='SELECT')
            bpy.ops.uv.smart_project(angle_limit=1.15, island_margin=0.001)
            assert bpy.ops.object.mode_set(mode='OBJECT') == {'FINISHED'}
        scene.unit_settings.system = 'METRIC'
        scene.unit_settings.scale_length = 1
        scene.world = bpy.data.worlds.new(name + '_World')
        scene.world.color = (.15,.15,.15)
        return obj


def safe(opened):
    model = Assembly()
    # Rotate an open-top shell so its opening becomes the safe's front.
    shell = Assembly()
    shell.hollow_box(.90, 1.10, .75, .075)
    transform = Matrix.Translation((0,.375,.55)) @ Matrix.Rotation(math.pi/2,4,'X')
    model.add(shell.vertices, shell.faces, 0, transform)
    hinge = Vector((-.36,-.41,.55))
    door = Matrix.Identity(4)
    if opened:
        door = Matrix.Translation(hinge) @ Matrix.Rotation(math.radians(-110),4,'Z') @ Matrix.Translation(-hinge)
    model.box((0,-.405,.55),(.70,.075,.91),0,door)
    model.box((0,-.448,.55),(.57,.018,.78),1,door)
    model.box((0,-.461,.55),(.52,.016,.73),0,door)
    for z in (.19,.90):
        model.cylinder((-.377,-.414,z),.035,.14,axis='Z')
    model.cylinder((0,-.50,.56),.047,.10,transform=door)
    for i in range(4):
        a = i*math.pi/2
        center = (.085*math.cos(a),-.56,.56+.085*math.sin(a))
        rotation = Matrix.Translation(center) @ Matrix.Rotation(-a,4,'Y')
        model.box((0,0,0),(.16,.025,.022),1,door @ rotation)
    # A segmented torus forms a readable wheel at close and game-camera range.
    vertices=[]
    for i in range(20):
        a=i*math.tau/20
        for j in range(6):
            b=j*math.tau/6
            r=.16+.013*math.cos(b)
            vertices.append((r*math.cos(a),-.56+.013*math.sin(b),.56+r*math.sin(a)))
    faces=[(i*6+j,((i+1)%20)*6+j,((i+1)%20)*6+(j+1)%6,i*6+(j+1)%6) for i in range(20) for j in range(6)]
    model.add(vertices,faces,1,door)
    for x in (-.235,.235):
        for z in (.23,.86):
            model.cylinder((x,-.477,z),.014,.018,transform=door,segments=8)
    if opened:
        for x in (-.18,0,.18):
            model.box((x,-.05,.12),(.15,.32,.08),1)
    return model.make('SM_CityBankSafe'+('Open' if opened else 'Closed'),['M_ExplorerSteel','M_ExplorerBrass'])


def chest(opened):
    model = Assembly()
    model.hollow_box(1.12,.72,.45,.055)
    hinge=Vector((0,.36,.45))
    lid=Matrix.Translation(hinge) @ Matrix.Rotation(math.radians(-105),4,'X') @ Matrix.Translation(-hinge) if opened else Matrix.Identity(4)
    model.curved_lid(1.15,.74,.28,.452,.035,transform=lid)
    for x in (-.55,.55):
        vertices=[]
        for edge in (-.018,.018):
            for i in range(13):
                a=math.pi*i/12
                vertices.append((x+edge,.345*math.cos(a),.452+.255*math.sin(a)))
        faces=[tuple(range(12,-1,-1)),tuple(range(13,26))]
        faces += [(i,(i+1)%13,(i+1)%13+13,i+13) for i in range(13)]
        model.add(vertices,faces,0,lid)
    for x in (-.39,.39):
        model.curved_lid(.055,.758,.292,.452,.018,1,lid,x)
        for y in (-.364,.364):
            model.box((x,y,.23),(.055,.018,.43),1)
        for z in (.10,.36):
            model.cylinder((x,-.382,z),.014,.018,segments=8)
    for x in (-.55,.55):
        model.box((x,0,.06),(.06,.74,.08),1)
    model.box((0,-.386,.40),(.10,.04,.16),1)
    model.cylinder((0,-.419,.39),.025,.025,segments=12)
    for x in (-.28,.28):
        model.cylinder((x,.36,.46),.027,.09,axis='Z')
    if opened:
        for x in (-.30,-.10,.10,.30):
            model.box((x,0,.11),(.17,.35,.10),1)
        for x in (-.25,0,.25):
            model.cylinder((x,-.15,.175),.065,.025,axis='Z',segments=12)
    return model.make('SM_CityMarketCrate'+('Open' if opened else 'Closed'),['M_ExplorerWood','M_ExplorerBrass'])


objects = [safe(False), safe(True), chest(False), chest(True)]
manifest = {'materials': {name: {'color': list(color), 'metallic': metallic, 'roughness': roughness}
                          for name,(color,metallic,roughness) in palette.items()},
            'objects': [obj.name for obj in objects]}
(output/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
bpy.ops.wm.save_as_mainfile(filepath=str(output/'ExplorationProps.blend'))
print(json.dumps({'saved':str(output/'ExplorationProps.blend'),'objects':[o.name for o in objects]}))

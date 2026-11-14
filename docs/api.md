# 公共 API 与数据约定

## High-level

`load(path, options)` / `load_string(text, filename, options)` 返回拥有全部数据的 `ParseResult`。`bool(result)` 仅当没有 error 为真；warning 不影响成功。失败时可能含部分 Scene，不能当作有效场景使用。`format(Diagnostic)` 输出 `filename:line:column: severity [code] message`。

默认材质位于 `materials[0]`，类型 diffuse；默认 Camera/Film/Sampler 等具有官方默认类型。未声明实体的 location.filename 为空。`scene.directives` 保留原始声明事件，实体的 `parameters` 保留声明值；Attribute 的默认参数单列于 `inherited_parameters`，`find_parameter(entity, name)` 优先读取最后声明值。API 不承诺稳定 ABI。

`Scene::shapes` 包含世界 Shape 和对象定义中的 Shape；只遍历 `world_shapes` 与 `instances` 才得到真正放置的内容。对象定义里的 Shape 不自动放到世界。向量索引 `Id` 不受 vector 内存重分配影响；`invalid_id` 表示没有绑定。

NamedMaterial、ObjectInstance、介质引用及 texture 参数在 finish 时解析。Texture 有 float/spectrum 两个名称空间；没有材质/纹理插件类型 schema 时，一个同名引用可能含 `id` 和 `alternate_id`，消费者按参数期望类型选择。库不实例化纹理、材质、BSDF 或介质算法，也不完整验证其类型专属参数。Mix 的 `materials` 字符串引用也会解析。

`ResourceReference` 保存原始 literal、根目录下的候选绝对路径和来源。Spectrum 字符串既可能是预定义光谱名，也可能是文件，`spectrum_name_or_file` 明确保留这种歧义；核心不会假装它一定是一个存在的文件。Film 的输出 filename 也是路径声明，IR 不强制输入/输出文件存在。

## Low-level callback

```cpp
struct Target : pbrt4::ParserTarget {
    void on_directive(const pbrt4::Directive& d) override {
        // d.keyword, d.arguments, d.parameters, d.location
    }
    void on_file_begin(const pbrt4::FileEvent& e) override {
        // Distinguish root / include / import. Import is not text substitution.
    }
};
Target target;
auto result = pbrt4::parse_string("WorldBegin Shape \"sphere\"", target);
```

Callback 参数只在调用内借用，但所有事件字段拥有字符串，可以安全复制保留。Tokenizer 也拥有输入；返回的 Token 文本不因 next() 失效。当前 tokenizer 忽略注释，不是保留空白/注释的格式化工具。line/column 从 1 开始，column 按输入字节计数。非法 UTF-8 返回诊断；UTF-8 名称不做 NFC 规范化。`path_from_utf8` / `path_to_utf8` 在 Windows 与 Linux 间转换路径；PBRT 文件名与资源字符串统一视为 UTF-8。

语法错误后停止当前 parse，返回诊断；此前发出的事件不会回滚。发生错误时文件 begin/end 不保证配对，应依据 SyntaxResult 错误清理自有消费者。`SceneBuilder` 用一次 `finish(syntax_result.diagnostics)` 产生最终结果。

## 路径与虚拟文件系统

```cpp
pbrt4::LoadOptions options;
options.base_directory = "/virtual/scene";
options.read_file = [&](const std::filesystem::path& absolute_path)
    -> std::optional<std::string> {
    // Return UTF-8 scene bytes, or nullopt for missing file.
    return virtual_files.read(absolute_path);
};
auto result = pbrt4::load("/virtual/scene/root.pbrt", options);
```

根文件、Include 和 Import 均走 read_file；内存入口根内容来自调用者，其依赖走 read_file。默认单文件上限 256 MiB、文件栈最多 64 层（含根文件）。循环通过规范化路径检测。gzip 返回明确诊断。路径无 cwd fallback；本库不设置全局搜索目录。GeometryOptions.resources.read_file 可以读取包含 NUL 的二进制 PLY。

Import 在世界块中继承 CTM、材质、面积光、介质、Attribute 默认值及颜色空间，在独立分支中运行。新名称对父文件/兄弟 Import 不可见；父分支名称可在子分支引用。它不继承命名坐标系，栈必须在本文件分支内平衡。Include 则共享全部状态，可以跨 Include 边界平衡 Attribute。

## Geometry

```cpp
pbrt4::GeometryOptions options;
options.tessellate_spheres = true;
auto geometry = pbrt4::extract_geometry(result.scene, options);
if (!geometry) { /* inspect geometry.diagnostics */ }
for (const auto& instance : geometry.instances) {
    const auto& mesh = geometry.meshes.at(instance.mesh);
    // mesh stays in source object space; compose instance transforms in renderer.
}
```

每个被使用的 Shape 只提取一个局部 mesh；实例保留材质、面积光、源 Shape ID、ReverseOrientation 和动画端点。当前只支持 trianglemesh、PLY、显式球体离散化。其他 Shape 返回 error，不会静默删除。缺失的法线/UV 保持为空；球体离散化生成的属性属于派生几何，不写回 Scene IR。

`bake_instance` 是显式复制和变换：仅接受可逆仿射静态矩阵，按逆转置处理法线，对负行列式和 ReverseOrientation 调整绕序。动画仍保留在 IR，烘焙明确拒绝而不取 t=0。核心没有动画插值算法。

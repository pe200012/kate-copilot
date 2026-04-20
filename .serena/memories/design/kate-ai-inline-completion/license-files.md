2026-04-20：新增 LGPL 许可证文件与 README 标注。

- 新增 `LICENSE`：GNU Library General Public License v2 (LGPL-2.0) 完整文本，覆盖 `LGPL-2.0-or-later` SPDX 标识的主授权。
- `README.md` 新增 License 小节：声明 `LGPL-2.0-or-later` 并链接到 `LICENSE`。
- 验证：`cmake --build build -j 8 && ctest --test-dir build --output-on-failure` => 12/12 passed。
Gentoo live ebuild 已写入仓库：`packaging/gentoo/app-editors/kate-copilot/`。

- `kate-copilot-9999.ebuild`
  - `inherit ecm git-r3`
  - `KFMIN=6.0.0`, `ECM_NONGUI=true`, `ECM_TEST=true`, `VIRTUALX_REQUIRED=test`
  - `EGIT_REPO_URI=https://github.com/pe200012/kate-copilot.git`, `EGIT_BRANCH=master`
  - RDEPEND: `dev-qt/qtbase:6[network,widgets]`, `kde-frameworks/{kconfig,kcoreaddons,ki18n,ktexteditor,kwallet}:6`
  - `src_configure`: `-DBUILD_TESTING=$(usex test ON OFF)`
  - `pkg_postinst`: 提示在 Kate 中启用插件
- `metadata.xml`：upstream GitHub remote-id

说明：ecm.eclass 会注入 gentoo_ecm_config.cmake，强制 `KDE_INSTALL_USE_QT_SYS_PATHS=ON` 与正确的 `KDE_INSTALL_LIBDIR`，使 KTextEditor 插件安装到 Qt6 系统插件目录（`.../qt6/plugins/kf6/ktexteditor`）。
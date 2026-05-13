# Copyright 2026 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=8

KFMIN=6.0.0
ECM_NONGUI=true
ECM_TEST=true
VIRTUALX_REQUIRED=test

inherit ecm git-r3

DESCRIPTION="Copilot-like AI inline completion plugin for Kate (KTextEditor)"
HOMEPAGE="https://github.com/pe200012/kate-copilot"
EGIT_REPO_URI="https://github.com/pe200012/kate-copilot.git"
EGIT_BRANCH="master"

LICENSE="LGPL-2.0-or-later"
SLOT="0"
KEYWORDS=""

RDEPEND="
	dev-qt/qtbase:6[network,widgets]
	kde-frameworks/kconfig:6
	kde-frameworks/kcoreaddons:6
	kde-frameworks/ki18n:6
	kde-frameworks/ktexteditor:6
	kde-frameworks/kwallet:6
	kde-frameworks/kxmlgui:6
"
DEPEND="${RDEPEND}"

src_configure() {
	local mycmakeargs=(
		-DBUILD_TESTING=$(usex test ON OFF)
	)
	ecm_src_configure
}

pkg_postinst() {
	ecm_pkg_postinst
	elog "Enable the plugin in Kate: Settings -> Configure Kate -> Plugins -> AI Inline Completion"
}

#!/usr/bin/env bash

set -e

case "$BUILD_TARGET" in
"vita")
	docker exec vitasdk /bin/bash -c "cd build && make -j4"
	;;
"switch")
	docker exec switchdev /bin/bash -c "cd build && make -j4"
	;;
"mac")
	cp -r res/maps ./build
	cp -r res/manual ./build
	cd build
	make -j4
	make install
	echo "Creating disk image"
	hdiutil create -volname Claudius -srcfolder claudius.app -ov -format UDZO claudius.dmg
	;;
"flatpak")
	flatpak-builder repo res/com.github.dathannobrega.claudius.json --install-deps-from=flathub --keep-build-dirs
	cp .flatpak-builder/build/claudius/res/version.txt res/version.txt
	;;
"appimage")
	cp -r res/maps ./build
	cp -r res/manual ./build
	cd build
	make -j4
	make DESTDIR=AppDir install
	cd ..
	./.ci_scripts/package_appimage.sh
	;;
"linux")
	if [ -d res/packed_assets ]
	then
		cp -r res/packed_assets ./build/assets
	else
		cp -r res/assets ./build
	fi
	cp -r res/maps ./build
	cp -r res/manual ./build
	cd build && make -j4
	;;
"emscripten")
	cd build && make -j4
	;;
*)
	cd build && make -j4 && make
	;;
esac

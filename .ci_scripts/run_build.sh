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
	if [[ "$GITHUB_REF" =~ ^refs/tags/v ]]
	then
		zip -r claudius.zip claudius.dmg maps manual
	else
		zip -r claudius.zip claudius.dmg 	
	fi
	;;
"ios")
	cd build
	xcodebuild clean build CODE_SIGN_IDENTITY="" CODE_SIGNING_REQUIRED=NO -scheme claudius
	;;
"flatpak")
	flatpak-builder repo res/com.github.keriew.claudius.json --install-deps-from=flathub --keep-build-dirs
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
	if [[ "$GITHUB_REF" =~ ^refs/tags/v ]]	
	then
		zip -r claudius.zip . -i claudius.AppImage maps manual
	else
		zip -r claudius.zip . -i claudius.AppImage	
	fi
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
	if [[ "$GITHUB_REF" =~ ^refs/tags/v ]]
	then
		zip -r claudius.zip claudius assets maps manual
	else
		zip -r claudius.zip claudius
	fi
	;;
"android")
	cd android
	if [ ! -f claudius.keystore ]
	then
		COMMAND=assembleDebug
		BUILDTYPE=debug
	else
		COMMAND=assembleRelease
		BUILDTYPE=release
	fi
	echo "Running ./gradlew $COMMAND"
	TERM=dumb ./gradlew $COMMAND
	if [ -f claudius/build/outputs/apk/release/claudius-release.apk ]
	then
		cp claudius/build/outputs/apk/release/claudius-release.apk ../build/claudius.apk
	fi
	cd ..
	if [ ! -f "deps/SDL2-$BUILDTYPE.aar" ]
        then
		cp android/SDL2/build/outputs/aar/SDL2-$BUILDTYPE.aar deps/SDL2-$BUILDTYPE.aar
	fi
	;;
"emscripten")
	cd build && make -j4
	;;
*)
	cd build && make -j4 && make
	;;
esac

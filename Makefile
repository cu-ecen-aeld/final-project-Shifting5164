.SILENT:
#.PHONY:

#SHELL := /bin/bash

DOCKERTAG=projenv
DIR_DEBUG=build/debug
DIR_RELEASE=build/release
PROJ_NAME=cewserver

all: submodule libs release
pipeline_test: submodule libs test
pipeline_test_mem: submodule libs test_mem
pipeline_test_mem_hist: submodule libs pipeline_test_mem_hist
pipeline_check: submodule libs debug check


submodule:
	git config --global --add safe.directory '*'
	git config --local status.showUntrackedFiles no
	git submodule update --init --recursive --force --checkout
	git submodule
	git diff ./external/

docker_build:
	cd Docker && docker build --tag ${DOCKERTAG} .

docker_run:
	docker run -p 9000:9000 \
		--privileged \
		--rm \
		--volume $(shell pwd):/work \
		--workdir /work \
		--interactive \
		--tty \
		${DOCKERTAG}

#https://clang.llvm.org/docs/HowToSetupToolingForLLVM.html
#https://stackoverflow.com/questions/48625499/cppcheck-support-in-cmake
check: debug
	echo "\n#################################"
	echo "clang-check:"
	echo "#################################"
	clang-check -p ./build/debug/ --analyze ./src/* ./include/*

	echo "\n#################################"
	echo "cppcheck:"
	echo "#################################"
	cppcheck --error-exitcode=1 \
		--enable=all \
		--std=c11 \
		--suppress=missingIncludeSystem \
		--suppress=unusedFunction \
		--suppress=*:external/* \
		--project=./build/debug/compile_commands.json

clean:
	-rm --force --recursive -- ./build/*

#Note: cmake release / debug configs
#https://stackoverflow.com/questions/7724569/debug-vs-release-in-cmake
#https://clang.llvm.org/docs/HowToSetupToolingForLLVM.html
#https://stackoverflow.com/questions/48625499/cppcheck-support-in-cmake
debug:
	mkdir --parents -- ${DIR_DEBUG}
	cmake -S . -B ${DIR_DEBUG} -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	cmake --build ${DIR_DEBUG} --config Debug --verbose --clean-first
	file ${DIR_DEBUG}/${PROJ_NAME}
	checksec --file=${DIR_DEBUG}/${PROJ_NAME}

debug_strace: debug
	strace -f -e 'trace=!clock_nanosleep' -s1000 -y ./build/debug/cewserver

debug_mem: debug
	valgrind --malloc-fill=0xAB --error-exitcode=1 --leak-check=full --track-origins=yes --show-leak-kinds=all --num-callers=40 --trace-children=yes ./build/debug/cewserver_test

debug_run: debug
	./build/debug/cewserver

release:
	mkdir --parents -- ${DIR_RELEASE}
	cmake -S . -B ${DIR_RELEASE} -DCMAKE_BUILD_TYPE=Release
	cmake --build ${DIR_RELEASE} --config Release --verbose --clean-first
	file ${DIR_RELEASE}/${PROJ_NAME}
	checksec --file=${DIR_RELEASE}/${PROJ_NAME}

test: debug
	mkdir -p -- /var/tmp/cew_test/
	cp -r -- ./test/ini/ /var/tmp/cew_test/
	CMOCKA_TEST_ABORT='1' ./build/debug/cewserver_test

test_mem: debug
	mkdir -p -- /var/tmp/cew_test/
	cp -r -- ./test/ini/ /var/tmp/cew_test/
	valgrind --malloc-fill=0xAB --error-exitcode=1 --leak-check=full --track-origins=yes --show-leak-kinds=all --num-callers=40 --trace-children=yes ./build/debug/cewserver_test

test_mem_hist: debug
	mkdir -p -- /var/tmp/cew_test/
	cp -r -- ./test/ini/ /var/tmp/cew_test/
	valgrind --tool=massif ./build/debug/cewserver_test


libs: libini libev cmocka

#NOTE: https://cmake.org/cmake/help/latest/variable/BUILD_SHARED_LIBS.html
libini:
	cd external/libini \
	  && cmake -S . -B build -D BUILD_SHARED_LIBS=off\
	  && cmake --build build --config Release --verbose --clean-first

libev:
	cd external/libev \
	  && ./configure \
	  && make -j$(nproc)

cmocka:
	-rm --force --remove -- external/cmocka/build
	mkdir --parents -- external/cmocka/build
	cd external/cmocka/build \
		&& cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release .. \
		&& make -j$(nproc) \
		&& make install

gef:
	bash -c "$$(curl -fsSL https://gef.blah.cat/sh)"
	echo "** Installed **"

kill:
	./tools/killall

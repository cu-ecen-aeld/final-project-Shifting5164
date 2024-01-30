.SILENT:
.PHONY:

DOCKERTAG=projenv
DIR_DEBUG=build/debug
DIR_RELEASE=build/release
PROJ_NAME=cewserver

all: submodule libs release
pipeline_test: submodule libs cmocka test
pipeline_check: submodule libs debug check

submodule:
	git config --global --add safe.directory '*'
	git config --local status.showUntrackedFiles no
	git submodule update --init --recursive

docker_build:
	cd Docker && docker build --tag ${DOCKERTAG} .

docker_run:
	docker run -p 9000:9000 --privileged --rm --volume $(shell pwd):/work --workdir /work --interactive --tty  ${DOCKERTAG}

#https://clang.llvm.org/docs/HowToSetupToolingForLLVM.html
check:
	echo "\n#################################"
	echo "clang-check:"
	echo "#################################"
	clang-check -p ./build/debug/ --analyze ./src/*

	echo "\n#################################"
	echo "cppcheck:"
	echo "#################################"
	cppcheck --error-exitcode=1 --enable=all --std=c11 --suppress=missingIncludeSystem ./src/*

clean:
	-rm --force --recursive -- ./build/*

#Note: cmake release / debug configs
#https://stackoverflow.com/questions/7724569/debug-vs-release-in-cmake
#https://clang.llvm.org/docs/HowToSetupToolingForLLVM.html
debug:
	mkdir --parents -- ${DIR_DEBUG}
	cmake -S . -B ${DIR_DEBUG} -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	cmake --build ${DIR_DEBUG} --config Release --verbose --clean-first
	file ${DIR_DEBUG}/${PROJ_NAME}
	checksec --file=${DIR_DEBUG}/${PROJ_NAME}

release:
	mkdir --parents -- ${DIR_RELEASE}
	cmake -S . -B ${DIR_RELEASE} -D CMAKE_BUILD_TYPE=Release
	cmake --build ${DIR_RELEASE} --config Debug --verbose --clean-first
	file ${DIR_RELEASE}/${PROJ_NAME}
	checksec --file=${DIR_RELEASE}/${PROJ_NAME}

.PHONY: test
test:
	mkdir -p test/build
	cd test/ \
		&& cmake -S . -B build \
		&& cmake --build build --verbose --clean-first \
		&& build/cewserver_test

.PHONY: test_mem
test_mem:
	mkdir -p test/build
	cd test/ \
		&& cmake -S . -B build \
		&& cmake --build build --verbose --clean-first \
		&& build/cewserver_test \
		&& valgrind --leak-check=full --show-leak-kinds=all build/cewserver_test

libs: libini libev

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
	-rm -fr external/cmocka/build
	mkdir -p external/cmocka/build
	cd external/cmocka/build \
		&& cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release .. \
		&& make -j$(nproc) \
		&& make install


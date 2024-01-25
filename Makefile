.PHONY:
.SILENT:

DOCKERTAG=projenv
DIR_DEBUG=build/debug
DIR_RELEASE=build/release
PROJ_NAME=cewserver

all: submodule libini libev release

submodule:
	git config --global --add safe.directory /work
	git config --global --add safe.directory /work/external/libev
	git config --global --add safe.directory /work/external/libini
	git submodule update --init --recursive
	git config --local status.showUntrackedFiles no

docker_build:
	cd Docker && docker build --tag ${DOCKERTAG} .

docker_run:
	docker run  --rm --volume $(shell pwd):/work --workdir /work --interactive --tty  ${DOCKERTAG}

check:
	echo "\n#################################"
	echo "clang-check:"
	echo "#################################"

	clang-check --analyze ./src/*
	echo "\n#################################"
	echo "cppcheck:"
	echo "#################################"
	cppcheck --enable=all --std=c11 --suppress=missingIncludeSystem ./src/*

clean:
	-rm --force --recursive -- ./build/*

#Note: cmake release / debug configs
#https://stackoverflow.com/questions/7724569/debug-vs-release-in-cmake
debug:
	mkdir --parents -- ${DIR_DEBUG}
	cmake -S . -B ${DIR_DEBUG} -D CMAKE_BUILD_TYPE=Debug
	cmake --build ${DIR_DEBUG} --config Release --verbose --clean-first
	file ${DIR_DEBUG}/${PROJ_NAME}
	checksec --file=${DIR_DEBUG}/${PROJ_NAME}

release:
	mkdir --parents -- ${DIR_RELEASE}
	cmake -S . -B ${DIR_RELEASE} -D CMAKE_BUILD_TYPE=Release
	cmake --build ${DIR_RELEASE} --config Debug --verbose --clean-first
	file ${DIR_RELEASE}/${PROJ_NAME}
	checksec --file=${DIR_RELEASE}/${PROJ_NAME}

#NOTE: https://cmake.org/cmake/help/latest/variable/BUILD_SHARED_LIBS.html
libini:
	cd external/libini \
	  && cmake -S . -B build -D BUILD_SHARED_LIBS=off\
	  && cmake --build build --config Release --verbose --clean-first

libev:
	cd external/libev \
	  && ./configure \
	  && make -j$(nproc)

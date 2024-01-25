.PHONY:
.SILENT:

DOCKERTAG=projenv
DIR_DEBUG=/build/debug
DIR_RELEASE=/build/release
PROJ_NAME=cewserver

docker_build:
	cd Docker && docker build --tag ${DOCKERTAG} .

docker_run:
	docker run --rm --volume $(shell pwd):/work --workdir /work --interactive --tty  ${DOCKERTAG}

check:
	echo "\n#################################"
	echo "clang-check:"
	echo "#################################"

	clang-check ./src/*
	echo "\n#################################"
	echo "cppcheck:"
	echo "#################################"
	cppcheck ./src/*

clean:
	-rm --force --recursive -- ./build/*

#Note: cmake release / debug configs
#https://stackoverflow.com/questions/7724569/debug-vs-release-in-cmake
release:
	mkdir --parents -- ${DIR_DEBUG}
	cmake -S . -B ${DIR_DEBUG} -D CMAKE_BUILD_TYPE=Release
	cmake --build ${DIR_DEBUG} --config Release --verbose --clean-first
	file ${DIR_DEBUG}/${PROJ_NAME}
	checksec --file=${DIR_DEBUG}/${PROJ_NAME}

debug:
	mkdir --parents -- ${DIR_RELEASE}
	cmake -S . -B ${DIR_RELEASE} -D CMAKE_BUILD_TYPE=Debug
	cmake --build ${DIR_RELEASE} --config Debug --verbose --clean-first
	file ${DIR_RELEASE}/${PROJ_NAME}
	checksec --file=${DIR_RELEASE}/${PROJ_NAME}



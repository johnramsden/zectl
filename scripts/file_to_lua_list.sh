#!/bin/sh

zcp_dir=${2:-"data/zcp"}
zcp_header=${3:-"zcp.h"}

header() {
    printf "%s\n" '#ifndef ZECTL_ZCP_H'
    printf "%s\n\n" '#define ZECTL_ZCP_H'

    printf "%s\n" "// Generated, DO NOT EDIT BY HAND"

    for file in $(find ${zcp_dir} -type f -name '*.lua'); do
        [ -e "${file}" ] || break
        echo "extern const char "'*'"zcp_$(basename ${file} | cut -f 1 -d '.');"
    done

    printf "\n%s\n" '#endif //ZECTL_ZCP_H'
}
cfile() {


    printf "%s\n" "// Generated, DO NOT EDIT BY HAND"
    printf "%s\n" "#include \"${zcp_header}\""

    for file in $(find ${zcp_dir} -type f -name '*.lua'); do
        [ -e "${file}" ] || break
        echo "const char "'*'"zcp_$(basename ${file} | cut -f 1 -d '.') = "
        while read -r line; do
            printf "\"%-80s\\\n\"\n" "$(echo ${line} | sed 's|"|\\"|g')";
        done < "${file}"
        printf "%s\n" ";"
    done

}

case ${1} in
	header)
		header
		;;
	c)
		cfile
		;;
	*)
		exit -1
		;;
esac


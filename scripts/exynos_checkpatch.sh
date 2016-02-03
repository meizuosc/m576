#
# exynos_cehckpatch.sh - a tool to test build, defconfig and patch format
#
# Dept    : S/W Solution Dev Team
# Author  : Solution3 Power Part
# Update  : 2014.12.08
#

#!/bin/bash

smdk7580_defconfig=smdk7580_defconfig

build_log_smdk7580=build_log_smdk7580
build_log_univ5433=build_log_univ5433
build_log_univ7420=build_log_univ7420
build_log_univ7580=build_log_univ7580

def_log_smdk7580=def_log_smdk7580
def_log_univ7580=def_log_univ7580

checkpatch_helper=scripts/exynos_checkpatch_helper.py

print_log()
{
	echo -e "\033[31m""$1""\033[0m"
}

print_help_message()
{
	echo 'usage: ./scripts/exynos_checkpatch.sh [-b]'
	echo '                                      [-d]'
	echo '                                      [-c] [# of patch]'
	echo '                                      [-i] [testcase]'
	echo '                                      [-h or --help]'
	echo '                                      [# of patch]'
	echo ''
	echo 'Options are as follows:'
	echo '  -b    Run build test with univ5433, univ7420 and univ7580'
	echo '  -d    Run defconfig test with smdk7580 and univ7580'
	echo '  -c    Run checkpatch.pl test'
	echo '  -i    Run custom test with testcase file'
	echo '  -h    Print help message'
}

clean_kernel()
{
	make clean
	make distclean
}

build_kernel_and_dtb()
{
	clean_kernel
	make "$1"
	make -j 24 > "$2"
	make dtbs
}

build()
{
	build_kernel_and_dtb "$1" "$2"
	result=''
	if [ -e vmlinux ]; then
		result='1'
		rm "$2"
	else
		result='0'
	fi
	python ${checkpatch_helper} -b "$1" "$2" ${result}
}

run_build_test()
{
	if [ -e arch/arm64/configs/"$1" ]; then
		build "$1" "$2"
	else
		dummy_build_log='0xefefefef'
		python ${checkpatch_helper} -b "$1" ${dummy_build_log} '0'
	fi
}

compare_config()
{
	result=''
	if [ "$3" = "$5" ]; then
		result='1'
		rm "$2"
	else
		result='0'
	fi
	python ${checkpatch_helper} -d "$1" "$2" ${result}
}

run_defconfig_test()
{
	if [ -e arch/arm64/configs/"$1" ]; then
		clean_kernel
		make "$1"
		make savedefconfig
		diff -u defconfig arch/arm64/configs/"$1" > "$2"
		num_word=`wc -c "$2"`
		expected_num_word="0 "$2""
		compare_config $1 $2 ${num_word} ${expected_num_word}
		rm defconfig
	else
		dummy_def_log='0xfefefefe'
		python ${checkpatch_helper} -d "$1" ${dummy_def_log} '0'
	fi
}

run_checkpatch_test()
{
	python ${checkpatch_helper} -c "$1"
}

run_custom_test()
{
	exec < "$1"
	while read line ; do
		test_type=`echo ${line} | awk '{print $1}'`
		if [ ${test_type} = 'b' ]; then
			defconfig=`echo ${line} | awk '{print $2}'`
			build_log=`echo build_log_${defconfig}`
			run_build_test ${defconfig} ${build_log}
		elif [ ${test_type} = 'd' ]; then
			defconfig=`echo ${line} | awk '{print $2}'`
			def_log=`echo def_log_${defconfig}`
			run_defconfig_test ${defconfig} ${def_log}
		elif [ ${test_type} = 'c' ]; then
			num_patch=`echo ${line} | awk '{print $2}'`
			run_checkpatch_test ${num_patch}
		elif [ ${test_type} = '#' ]; then
			continue
		else
			print_log 'Testcase file is corrupted <- FAIL'
		fi
	done
}

run_default_test()
{
	run_build_test ${univ5433_defconfig} ${build_log_univ5433}
	run_build_test ${univ7420_defconfig} ${build_log_univ7420}
	run_build_test ${smdk7580_defconfig} ${build_log_smdk7580}
	run_build_test ${univ7580_defconfig} ${build_log_univ7580}
	run_defconfig_test ${smdk7580_defconfig} ${def_log_smdk7580}
	run_defconfig_test ${univ7580_defconfig} ${def_log_univ7580}
	run_checkpatch_test "$1"
}

main()
{
	if [ "$1" = '-b' ]; then
		run_build_test ${univ5433_defconfig} ${build_log_univ5433}
		run_build_test ${univ7420_defconfig} ${build_log_univ7420}
		run_build_test ${smdk7580_defconfig} ${build_log_smdk7580}
		run_build_test ${univ7580_defconfig} ${build_log_univ7580}
	elif [ "$1" = '-d' ]; then
		run_defconfig_test ${smdk7580_defconfig} ${def_log_smdk7580}
		run_defconfig_test ${univ7580_defconfig} ${def_log_univ7580}
	elif [ "$1" = '-c' ]; then
		run_checkpatch_test "$2"
	elif [ "$1" = '-i' ]; then
		run_custom_test "$2"
	elif [ "$1" = '-h' ] || [ "$1" = '--help' ]; then
		print_help_message
	else
		run_default_test "$1"
	fi
}

main "$1" "$2"

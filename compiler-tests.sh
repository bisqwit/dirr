runwith()
{
	rm *.o
	sed "s/#TOOL1/select_tool CXX $1/;
	     s/#TOOL2/select_tool CPP $2/;
	     s/#TOOL3/select_tool CC  $3/" < configure > configure.test
	chmod +x configure.test
	./configure.test && make -j8
	rm -f configure.test
}

#runwith g++-6 gcc-6 gcc-6
# ^Too early
#exit

runwith g++-7 gcc-7 gcc-7
runwith g++-8 gcc-8 gcc-8
runwith g++-9 gcc-9 gcc-9
runwith g++-10 gcc-10 gcc-10
runwith g++-11 gcc-11 gcc-11
runwith clang++-9 clang-9 clang-9
runwith clang++-10 clang-10 clang-10
runwith clang++-11 clang-11 clang-11
runwith clang++-12 clang-12 clang-12
runwith clang++-13 clang-13 clang-13

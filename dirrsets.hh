    /**************************************************************
	 * mode() - How to color the drwxr-xr-x string.
	 *
	 * ?               is the unknown case
	 * #               is for numeric modes
     " S,H,R,A         are for dos attributes
	 * l,d,x,c,b,p,s,- are the corresponding characters
	 *                 in the mode string respectively
	 * D               is door. It's used in Solaris.
     **************************************************************/

    "mode(SB,H8,R9,A3,DC,lB,d9,x9,cE,bE,p6,sD,-7,?C,#3)"

    /**************************************************************
     * info() - How to color the type characters
     *          in the end of filenames.
     *
     * / is for directories
     * * for executables
     * @ for links
     * & is for arrows in hardlinks
     * = for sockets
     * | for pipes
     * ? for links whose destinations does not exist
     *
     * Note: Colour 0 disables the character being printed.
     *
     **************************************************************/
    "info(/1,*2,@3,=5,|8,?C,&F)"

    /**************************************************************
     * owner() - How to color the file owner name.
     *
     * First is the color in the case of own file,
     * second for the case of file not owned by self.
     **************************************************************/
    "owner(4,8)"

    /**************************************************************
     * type() - How to color the file names, if it does
     *          not belong to any class of byext()'s.
     *
     * l=links, d=directories, x=executable
     * c=character devices, s=sockets
     * b=block devices, p=pipes.
     * D=door (used in Solaris).
     * ? is for file/link names which were not stat()able.
     **************************************************************/
    "type(lB,d9,xA,cE,bE,p6,sD,?3,DC)"

    /**************************************************************
     * descr() - How to color the <DIR>, <PIPE> etc texts
     **************************************************************/
    "descr(3)"

    /**************************************************************
     * date() - How to color the datetimes
     **************************************************************/
    "date(7)"

    /**************************************************************
     * size() - How to color the numeric file sizes
     **************************************************************/
    "size(7)"

#ifndef DJGPP
    /**************************************************************
	 * nrlink() - How to color the number of hard links
     **************************************************************/
    "nrlink(7)"
#endif
	
    /**************************************************************
	 * num() - How to color the numbers in total-sums
     **************************************************************/
    "num(3)"

#ifndef DJGPP
    /**************************************************************
     * group() - How to color the file owner name.
     *
     * First is the color in the case of file of own group,
     * second for the case of file of not belonging the group to.
     **************************************************************/
    "group(4,8)"
#endif

    /**************************************************************
	 * txt() - The default color used for all other texts
     **************************************************************/
    "txt(7)"

    /**************************************************************
     * byext() - How to color the filenames that match one or more
     *           of the patterns in the list
     *
     * The first argument is the color code. It may have 'i' at end
     * of it. If it has 'i', the file name patterns are case insensitive.
     *
     * There may be multiple byext() definitions.
     *
     * See the documentation at function WildMatch in this source
     * code for more information about the wildcards.
     **************************************************************/

    // green: object files
    "byext(2i *.so *.o *.vxd *.dll *.drv *.obj *.dll *.a)"
    // cyan: text files
    "byext(3i *.txt *.htm *.html *.xml *.xhtml *.1st *.wri *.ps *.doc readme quickstart install)"
    // red: unwanted temp files
    "byext(4i core DEADJOE)"
    // magenta: sound-only multimedia files
    "byext(5i *.mid *.mod *.mtm *.s3m *.xm* *.mp2 *.mp3 *.wav *.ogg *.smp *.au *.ult *.669 *.aac *.spc *.nsf)"
    // brown: programming
    "byext(6i *.bas *.pas *.php *.phtml *.pl *.rb *.c *.cpp *.cc *.asm *.s *.inc *.h *.hh *.pov *.irc *.hpp *.src *.ttt *.pp *.p makefile *.mak)"
    // dark gray: temp files
    "byext(8i *~ *.bak *.old *.bkp *.st3 *.tmp *.$$$ tmp* *.out *.~*)"
    // bright green: executable files
    "byext(Ai *.exe *.com *.bat *.sh)"
    // bright red: archives
    "byext(Ci *.tar *.*z *.bz2 *.zip *.arj *.lzh *.lha *.rar *.deb *.rpm *.arj *.7z *.lzma)"
    // bright magenta: image and video files
    "byext(Di *.gif *.bmp *.mpg *.mpeg *.mp4 *.avi *.ogm *.mkv *.asf *.x?m *.mov *.tga *.png *.tif *.wmv *.pcx *.lbm *.img *.jpg *.jpeg *.fl\\w)"

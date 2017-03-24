/*
Colors have the following format, in hexadecimal:

For legacy 16-color formats:
  color = blink*0x80 + background*0x10 + intensity*0x08 + foreground*0x01
  where blink      = 0..1
        background = 0..7
        intensity  = 0..1
        foreground = 0..7
  where colors are: 0 = black, 1 = blue,    2 = green, 3 = cyan,
                    4 = red,   5 = magenta, 6 = brown, 7 = white
  Example: C  = black background, bright red
           17 = blue background,  non-bright white foreground (light gray)

For xterm-256color formats:
  color = 0x100 + foreground*0x01 + background*0x200 + bold*0x20000
  where foreground = 0..255
        background = 0..255
  where colors are: 0..15 same as legacy 16-color formats
                  16..255 are:  16 + 36*red + 6*green + blue
                  where red=0..5, green=0..5, blue=0..5
  Example: 164 = greenish yellow with black background
*/

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

    "mode(SB,H8,R9,A3,DC,lB,d9,x9,cE,bE,p6,sD,-7,?C,#16D)"

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
     * type() - How to color the file names, if it does
     *          not belong to any class of byext()'s.
     *
     * l=links, d=directories, x=executable
     * c=character devices, s=sockets
     * b=block devices, p=pipes.
     * D=door (used in Solaris).
     * ? is for file/link names which were not stat()able.
     **************************************************************/
    "type(lB,d9,xA,cE,bE,p6,sD,?3,DC,-7)"

    /**************************************************************
     * descr() - How to color the <DIR>, <PIPE> etc texts
     **************************************************************/
    "descr(13D)"

    /**************************************************************
     * date() - How to color the datetimes
     **************************************************************/
    "date(7)"

    /**************************************************************
     * size() - How to color the numeric file sizes
     **************************************************************/
    "size(16E)"

    /**************************************************************
     * owner() - How to color the file owner name.
     *
     * First is the color in the case of own file,
     * second for the case of file not owned by self.
     **************************************************************/
    "owner(4,13B)"

#ifndef DJGPP
    /**************************************************************
     * group() - How to color the file owner name.
     *
     * First is the color in the case of file of own group,
     * second for the case of file of not belonging the group to.
     **************************************************************/
    "group(4,13B)"

    /**************************************************************
     * nrlink() - How to color the number of hard links
     **************************************************************/
    "nrlink(7)"
#endif

    /**************************************************************
     * num() - How to color the numbers in total-sums
     **************************************************************/
    "num(3)"

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
     * Wildcards recognized:
     *    ?     Matches any byte
     *    *     Matches zero or more bytes of anything
     *    \d    Matches any digit (0..9)
     *    \w    Matches any alphanumeric byte (0..9, A-Z, a-z)
     *    \x##  Matches byte with hexadecimal value ##
     *    \\    Matches the backslash
     *    [...] Matches a byte matching the range, with:
     *               ^    in the beginning of the range inverts the selection
     *               b-e  matches characters b,c,d,e
     *               \]   matches ]
     *               Anything else matches the letter verbatim
     **************************************************************/

    // green: object files
    R"(byext(2i *.so *.o *.vxd *.dll *.drv *.obj *.a *.lo *.la *.so.*))"
    // cyan: text files
    R"(byext(3i *.txt *.htm *.html *.xml *.xhtml *.1st *.wri *.ps *.doc *.docx *.odt))"
    R"(byext(3i readme quickstart install))"
    // red: unwanted temp files
    R"(byext(4 core DEADJOE))"
    // magenta: sound-only multimedia files
    R"(byext(5i *.mid *.mod *.mtm *.s3m *.xm* *.mp[23] *.wav *.ogg *.smp *.au *.ult *.669 *.aac *.spc *.nsf *.wma))"
    // brown: programming
    R"(byext(6i *.bas *.pas *.php *.phtml *.pl *.rb *.c *.cpp *.cc *.asm *.s *.inc *.h *.hh *.pov *.irc *.hpp))"
    R"(byext(6i *.src *.pp *.p *.mak *.in *.am))"
    R"(byext(6i makefile))"
    R"(byext(201BA configure *.sh))"
    // dark gray: temp files
    R"(byext(13Bi *~ *.bak *.old *.bkp *.st3 *.tmp *.$$$ tmp* *.out *.~*))"
    // bright green: executable files
    R"(byext(14Ei *.exe *.com *.bat))"
    // bright red: archives
    R"(byext(1A7i *.tar *.gz *.xz *.bz *.brotli *.bz2 *.zip *.arj *.lzh *.lha *.rar *.deb *.rpm *.7z *.lzma))"
    // bright magenta: image and video files
    R"(byext(Di *.gif *.bmp *.mpg *.mpeg *.mp4 *.avi *.ogm *.ogv *.mkv *.asf *.x?m *.mov *.tga *.png *.tif))"
    R"(byext(Di *.wmv *.pcx *.lbm *.img *.jpg *.jpeg *.fl\w *.rm *.tiff))"
    // red background and yellow: files with clear mistakes in filenames
    R"(byext(69D7 *\x13 *\?))"

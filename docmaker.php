<?php

# This is Bisqwit's generic docmaker.php, activated from depfun.mak.
# The same program is used in many different projects to create
# the README.html file from progdesc.php.
#
# docmaker.php version 1.0.3

# Copyright (C) 2000,2002 Bisqwit (http://iki.fi/bisqwit/)

foreach(array('progdesc.php', '/WWW/document.php') as $fn)
  if(!file_exists($fn))
  {
    print "$fn not found, not making document.\n";
    return 0;
  }

$content_array = file('progdesc.php');
$content = implode("", $content_array);
$fw = fopen('docmaker-temp.php', 'w');
fwrite($fw, preg_replace('/include.*;/U', '', $content));
fclose($fw);
include 'docmaker-temp.php';
unlink('docmaker-temp.php');

echo '<html><head><title>', htmlspecialchars($title), '</title>',
     '<style type="text/css"><!--', "
TABLE.toc {border:0px}
TD.toc   {font-size:80%; font-family:Tahoma; text-align:left}
H1       {font-size:250%; font-weight:bold} .level1 {text-align:center}
H2       {font-size:200%; font-weight:bold} .level2 {margin-left:1%}
H3       {font-size:160%; font-weight:bold} .level3 {margin-left:2%}
H4       {font-size:145%; font-weight:bold} .level4 {margin-left:3%}
H5       {font-size:130%; font-weight:bold} .level5 {margin-left:4%}
H6       {font-size:110%; font-weight:bold} .level5 {margin-left:5%}
BODY     {font-family:Arial,Verdana,Helvetica }
",   '--></style></head><body><h1>',
         htmlspecialchars($title), '</h1>',
     '<h2 class=level2> 0. Contents </h2>';

echo 'This is the documentation of ', htmlspecialchars($argv[1]), '.';

$url = 'http://oktober.stc.cx/source/'.rawurlencode($progname).'.html';
$k = 'The official home page of '.htmlspecialchars($progname).' is at '.
     '<a href="'.htmlspecialchars($url).'">'.htmlspecialchars($url).'</a>'.
     ' .<br>Check there for new versions.';
$text['download:99999. Downloading'] = $k;

include '/WWW/document.php';

$st1 = stat('progdesc.php');
$st2 = stat('docmaker.php');
echo '<p align=right><small>Generated from ',
         '<code>progdesc.php</code> (last updated: ', date('r', $st1[9]), ')',
         '<br>with ',
         '<code>docmaker.php</code> (last updated: ', date('r', $st2[9]), ')',
         '<br>at ', date('r'),
     '</small></p>';
         
echo '</body></html>';

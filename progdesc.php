<?php

$title = 'Directory lister';
$progname = 'dirr';

$todo='';foreach(file('/home/bisqwit/src/dirr/TODO') as $s)$todo.=$s;

$text = array(
   '1. Purpose' => "

List directories and files in *nix system. A colourful alternative
for /bin/ls . Not completely compatible with it, though.<br>
Given some luck, may compile on DOS too.

", '1. Copying' => "

dirr has been written by Joel Yliluoma, a.k.a.
<a href=\"http://bisqwit.iki.fi/\">Bisqwit</a>,<br>
and is distributed under the terms of the
<a href=\"http://www.gnu.org/licenses/licenses.html#GPL\">General Public License</a> (GPL).

", '1. To do' => "

<pre class=smallerpre>".htmlspecialchars($todo)."</pre>

", '1. Requirements' => "

GNU make is required.<br>

");
include '/WWW/progdesc.php';

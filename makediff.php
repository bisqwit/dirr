<?php

# This is Bisqwit's generic makediff.php, activated from depfun.mak.
# The same program is used in many different projects to create
# a diff file version history (patches).
#
# makediff.php version 1.0.2

# Copyright (C) 2000,2001 Bisqwit (http://iki.fi/bisqwit/)

# argv[1]: Newest archive if any
# argv[2]: Archive directory if any
# argv[3]: Disable /WWW/src linking if set

if(strlen($argv[2]))
{
  chdir($argv[2]);
  echo "\tcd ", $argv[2], "\n";
}

function ShellFix($s)
{
  return "'".str_replace("'", "'\''", $s)."'";
}

$fp=opendir('.');
$f=array();$i=0;
while(($fn = readdir($fp)))
{
  if(ereg('\.tar\.gz$', $fn))
    $f[$i++] = ereg_replace('\.tar\.gz$', '', $fn);

  #if(ereg('\.gz$', $fn))$f[$i++] = $fn;
}
closedir($fp);

function padder($k) { return str_pad($k, 9, '0', STR_PAD_LEFT); }
function cmp1($a, $b)
{
  $a1 = preg_replace('/([0-9]+)/e', "padder('\\1')", $a);
  $b1 = preg_replace('/([0-9]+)/e', "padder('\\1')", $b);
  if($a1 == $b1)return 0;
  if($a1 < $b1)return -1;
  return 1;
}
function cmp($a, $b)
{
  $k1 = ereg_replace('^patch-(.*)-[.0-9]*-([.0-9]*).*$', '\1-\2z', $a);
  $k2 = ereg_replace('^patch-(.*)-[.0-9]*-([.0-9]*).*$', '\1-\2z', $b);
  $k = cmp1($k1, $k2);
  if($k)return $k;
  return cmp1($a, $b);
}

usort($f, cmp);
#for($a=0; $a<$i; $a++)echo $f[$a], "\n";
#exit;

function Eexec($s)
{
  print "\t$s\n";
  exec($s);
}

$tmpdir = 'archives.tmp';

mkdir($tmpdir, 0700);
$prev = '';
$madeprev = 0;
$lastprog = '';
for($a=0; $a<$i; $a++)
{
  $this = $f[$a];
  
  chdir($tmpdir);
  
  $v1 = ereg_replace('^.*-([0-9])', '\1', $prev);
  $v2 = ereg_replace('^.*-([0-9])', '\1', $this);
  $prog = ereg_replace('-[0-9].*', '', $this);
  
  if(!strlen($lastprog))$lastprog = $prog;
  
  if(strlen($prev) && ($this == $argv[1] || !strlen($argv[1])) && $prog == $lastprog)
  {
    if(!$madeprev)
      Eexec('tar xfz ../'.shellfix($prev).'.tar.gz');
    Eexec('tar xfz ../'.shellfix($this).'.tar.gz');
    $madeprev = 1;

    $diffname = '../patch-'.$prog.'-'.$v1.'-'.$v2;
    Eexec('diff -NaHudr '.shellfix($prev).' '.shellfix($this).
          '|gzip -9 >'.shellfix($diffname).'.gz');
    Eexec('gzip -d <'.shellfix($diffname).'.gz|bzip2 -9 >'.shellfix($diffname).'.bz2');
    Eexec('rm -rf '.$prev);
    Eexec('touch -r../'.shellfix($this).'.tar.gz '.shellfix($diffname).'.{gz,bz2}');
    Eexec('chown --reference ../'.shellfix($this).'.tar.gz '.shellfix($diffname).'.{gz,bz2}');
    if(!$argv[3])
      Eexec('ln -f '.shellfix($diffname).'.{gz,bz2} /WWW/src/');
  }
  else
    $madeprev = 0;
  
  $lastprog = $prog;
  
  chdir('..');
  
  $prev = $this;
}

Eexec('rm -rf '.shellfix($tmpdir));

<?php

# argv[1]: Newest archive if any
# argv[2]: Archive directory if any
# argv[3]: Disable /WWW/src linking if set

if(strlen($argv[2]))
  chdir($argv[2]);

function ShellFix($s)
{
  return "'".str_replace("'", "'\''", $s)."'";
}
  
$fp=opendir('.');
$f=array();$i=0;
while(($fn = readdir($fp)))
  if(ereg('\.tar\.gz$', $fn))
    $f[$i++]=$fn;
closedir($fp);
sort($f);

function Eexec($s)
{
  print "\t$s\n";
  exec($s);
}

$tmpdir = 'archives.tmp';

mkdir($tmpdir, 0700);
$prev = '';
$madeprev = 0;
for($a=0; $a<$i; $a++)
{
  $this = ereg_replace('\.tar\.gz$', '', $f[$a]);
  
  chdir($tmpdir);
  
  if(strlen($prev) && ($this == $argv[1] || !strlen($argv[1])))
  {
    if(!$madeprev)
      Eexec('tar xfz ../'.shellfix($prev).'.tar.gz');
    Eexec('tar xfz ../'.shellfix($this).'.tar.gz');
    $madeprev = 1;

    $v1 = ereg_replace('^.*-', '', $prev);
    $v2 = ereg_replace('^.*-', '', $this);
    $prog = ereg_replace('-.*', '', $this);
    
    $diffname = '../patch-'.$prog.'-'.$v1.'-'.$v2;
    Eexec('diff -NaHudr '.shellfix($prev).' '.shellfix($this).
          '|gzip -9 >'.shellfix($diffname).'.gz');
    Eexec('gzip -d <'.shellfix($diffname).'.gz|bzip2 -9 >'.shellfix($diffname).'.bz2');
    Eexec('rm -rf '.$prev);
    Eexec('touch -r../'.shellfix($f[$a]).' '.shellfix($diffname).'.{gz,bz2}');
    Eexec('chown --reference ../'.shellfix($f[$a]).' '.shellfix($diffname).'.{gz,bz2}');
    if(!$argv[3])
      Eexec('ln -f '.shellfix($diffname).'.{gz,bz2} /WWW/src/');
  }
  else
    $madeprev = 0;
  
  chdir('..');
  
  $prev = $this;
}

Eexec('rm -rf '.shellfix($tmpdir));

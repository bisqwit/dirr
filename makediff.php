<?php

# This is Bisqwit's generic makediff.php, activated from depfun.mak.
# The same program is used in many different projects to create
# a diff file version history (patches).
#
# makediff.php version 1.0.9

# Copyright (C) 2000,2002 Bisqwit (http://iki.fi/bisqwit/)

# argv[1]: Newest archive if any
# argv[2]: Archive directory if any
# argv[3]: Disable /WWW/src linking if set

if($REMOTE_ADDR)
{
  header('Content-type: text/plain');
?>
This test was added due to abuse :)
Looks like somebody's friends had fun.

xx.yy.cam.ac.uk - - [20/Nov/2001:02:17:51 +0200] "GET /src/tmp/makediff.php HTTP/1.1" 200 36858 "http://www.google.com/search?q=%22chdir%3A+No+such+file+or+directory%22&btnG=Google+Search" "Mozilla/5.0 (X11; U; Linux i686; en-US; rv:0.9.5) Gecko/20011012"
207.195.43.xx - - [20/Nov/2001:02:18:18 +0200] "GET /src/tmp/makediff.php HTTP/1.1" 200 36858 "-" "Mozilla/4.0 (compatible; MSIE 5.0; Windows NT 5.1) Opera 5.12  [en]"
xx.yy.mi.home.com - - [20/Nov/2001:02:18:39 +0200] "GET /src/tmp/makediff.php HTTP/1.0" 200 36817 "-" "Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1)"
proxy1.slnt1.on.home.com - - [20/Nov/2001:02:18:53 +0200] "GET /src/tmp/makediff.php HTTP/1.0" 200 36817 "-" "Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; MSOCD; AtHome021SI)"
xx-yy-ftw-01.cvx.algx.net - - [20/Nov/2001:02:19:01 +0200] "GET /src/tmp/makediff.php HTTP/1.1" 200 36858 "-" "Mozilla/4.0 (compatible; MSIE 5.0; Windows 98; DigExt; Hotbar 3.0)"
  
I still keep this text here for Google to index this happily.

ChDir: no such file or directory - have fun ;)

(The hostnames have been masked because the original person contacted
 me and explained me what was it about. Wasn't an abuse try.)
<?
  exit;
}

ob_implicit_flush(true);

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
$f=array();
while(($fn = readdir($fp)))
{
  if(ereg('\.tar\.gz$', $fn))
    $f[] = ereg_replace('\.tar\.gz$', '', $fn);
  elseif(ereg('\.tar\.bz2$', $fn))
    $f[] = ereg_replace('\.tar\.bz2$', '', $fn);
}
closedir($fp);
$f = array_unique($f);

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

function Eexec($s)
{
  print "\t$s\n";
  return exec($s);
}

function FindInodes($directory)
{
  $inodes = array();
  $fp = @opendir($directory);
  if(!$fp)
  {
    print "OPENDIR $directory failed!\n";
    exit;
  }
  
  while(($fn = readdir($fp)))
  {
    if($fn=='.' || $fn=='..')continue;
    
    $fn = $directory.'/'.$fn;
    if(!is_link($fn))
    {
      $st = stat($fn);
      
      $inodes[$st[0].':'.$st[1]] = $fn;
    }
    if(is_dir($fn))
    {
      $inodes = $inodes + FindInodes($fn);
    }
  }
  closedir($fp);
  return $inodes;
}

function EraLinks($directory, &$inomap)
{
  $links = array();
  $fp = @opendir($directory);
  if(!$fp)
  {
    print "OPENDIR $directory failed!\n";
    exit;
  }
  while(($fn = readdir($fp)))
  {
    if($fn=='.' || $fn=='..')continue;
    
    $fn = $directory.'/'.$fn;
    if(is_link($fn))
    {
      $st = stat($fn); /* See if the target has already been included */
      if($inomap[$st[0].':'.$st[1]])
        $links[$fn] = $fn;
    }

    if(is_dir($fn))
      $links = $links + EraLinks($fn, $inomap);
  }
  closedir($fp);
  return $links;
}

$tmpdir = 'archives.tmp'; /* Subdir to not overwrite anything */

$tmpdir2= 'abcdefg.tmp';  /* Subdir to get the archive name safely
                           * This directory must not exist within
                           * the archive being analyzed.
                           */

mkdir($tmpdir, 0700);
mkdir($tmpdir.'/'.$tmpdir2, 0700);
$prev = '';
$madeprev = 0;
$lastprog = '';
foreach($f as $this)
{
  chdir($tmpdir);
  
  $v1 = ereg_replace('^.*-([0-9])', '\1', $prev);
  $v2 = ereg_replace('^.*-([0-9])', '\1', $this);
  $prog = ereg_replace('-[0-9].*', '', $this);
  
  if(!strlen($lastprog))$lastprog = $prog;
  
  if(strlen($prev) && ($this == $argv[1] || !strlen($argv[1])) && $prog == $lastprog)
  {
    if(!$madeprev)
    {
      chdir($tmpdir2);
      print 1;
      if(file_exists('../../'.$prev.'.tar.gz'))
        Eexec('gzip -d < ../../'.shellfix($prev).'.tar.gz | tar xf -');
      else
        Eexec('bzip2 -d < ../../'.shellfix($prev).'.tar.bz2 | tar xf -');
      $prevdirs = exec('echo *');
      exec('mv * ../');
      chdir('..');
    }
    if(1) /* Always true. Just for symmetry with the previous thing. */
    {
      $madeprev = 1;
      chdir($tmpdir2);
      $thisfn = '../'.$this.'.tar.gz';
      print 2;
      if(file_exists('../'.$thisfn))
        Eexec('gzip -d < ../'.shellfix($thisfn).' | tar xf -');
      else
      {
        $thisfn = '../'.$this.'.tar.bz2';
        Eexec('bzip2 -d < ../'.shellfix($thisfn).' | tar xf -');
      }
      $thisdirs = exec('echo *');
      exec('mv * ../');
      chdir('..');
    }

    $diffname = '../patch-'.$prog.'-'.$v1.'-'.$v2;
    
    $diffcmd = 'diff -NaHudr';
    
    $inomap = FindInodes($thisdirs) + FindInodes($prevdirs);
    $links = EraLinks($thisdirs, $inomap) + EraLinks($prevdirs, $inomap);
    $rmcmd = '';
    foreach($links as $linkname)
      $rmcmd .= shellfix($linkname).' ';
    if(strlen($rmcmd))
    {
      /* Erase files which of targets are already being diffed */
      Eexec('rm -f '.$rmcmd);
    }

    # foreach($exclusions as $ex)
    #   $diffcmd .= ' -x '.shellfix($ex);
    
    $diffcmd .= ' '.shellfix($prevdirs);
    $diffcmd .= ' '.shellfix($thisdirs);
    
    Eexec($diffcmd.
          '|gzip -9 >'.shellfix($diffname).'.gz');
    Eexec('gzip -d <'.shellfix($diffname).'.gz|bzip2 -9 >'.shellfix($diffname).'.bz2');
    Eexec('rm -rf '.$prev);
    Eexec('touch -r'.$thisfn.' '.shellfix($diffname).'.{gz,bz2}');
    Eexec('chown --reference '.$thisfn.' '.shellfix($diffname).'.{gz,bz2}');
    if(!$argv[3])
      Eexec('ln -f '.shellfix($diffname).'.{gz,bz2} /WWW/src/');
  }
  else
    $madeprev = 0;
  
  $lastprog = $prog;
  
  chdir('..');
  
  $prev     = $this;
  $prevdirs = $thisdirs;
}

exec('rm -rf '.shellfix($tmpdir));
echo "\tcd ..\n";

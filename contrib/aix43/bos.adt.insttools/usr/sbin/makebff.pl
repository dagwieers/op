#!/usr/bin/perl -w
# IBM_PROLOG_BEGIN_TAG 
# This is an automatically generated prolog. 
#  
# bos52S src/bos/usr/sbin/install/insttools/makebff.pl 1.9.1.1 
#  
# Licensed Materials - Property of IBM 
#  
# COPYRIGHT International Business Machines Corp. 2003,2008 
# All Rights Reserved 
#  
# US Government Users Restricted Rights - Use, duplication or 
# disclosure restricted by GSA ADP Schedule Contract with IBM Corp. 
#  
# IBM_PROLOG_END_TAG 
# @(#)10        1.9.1.1  src/bos/usr/sbin/install/insttools/makebff.pl, cmdmkinstallp, bos52S, s2008_51A3 9/30/08 16:43:46
use strict;

my $CONFIGDIR="./.info";  # Directory containing all the fileset config files
my $MESG="";              # Message for dspmsg
my $STRING="";            # Status string

# Maps associating filesets with their variables
my %bosboot=();           # BOS Boot flag
my %description=();
my %laf=();               # License Agreement File
my %lar=();               # License Agreement Requirement
my %requisite=();
my %version=();           # VRMF

# Globals
my $configFileBlocks=0;   # Number of blocks for all cfg files (for INSTWORK)
my @copyrights=();        # CRs go into the archive first, so keep a list of them
my $fileset="";
my $filesetDir="";        # Part of the dir for storing update fileset liblpp.a
my $hasRoot=0;            # Keep track of whether or not we have a ROOT part
my $hasRootFiles=0;       # Keep track of whether or not we have ROOT part files
my $hasUsrFiles=0;        # Keep track of whether or not we have USR part files
my $instrootDir="";       # Part of the dir for storing ROOT part files in an update's liblpp.a
my @liblpp=();            # Update pkgs may have more than one liblpp
my @listFileLine=();      # One line of list file data
my $listFileLineNum=0;
my $numFiles=0;           # Total number of files for this package
my $package="";
my $packageType="";       # I - regular, S - update
my $packageVersion="";    # Pkg VRMF
my %rootSize=();          # Maps ROOT part directories with their sizes for each fileset
my %size=();              # Maps USR part directories with their sizes for each fileset
my $symlinkKey=0;
my @updateLibDirs=();     # Need the liblpp.a directories for listing in the backup tree
my $vrmfDir="";           # Part of the dir for storing update fileset liblpp.a

my @usrFilesForArchive=();    # USR part config files from all filesets for the non-update liblpp.a
my @rootFilesForArchive=();   # ROOT part config files from all filesets for the non-update liblpp.a
my @configFiles=(
  "al","cfginfo","cfgfiles","config","config_u","copyright","err","fixdata",
  "inventory","namelist","odmadd","odmdel","post_i","post_u","pre_d","pre_i",
  "pre_rm","pre_u","productid","README","rm_inv","size","trc","unconfig","unconfig_u",
  "unodmadd","unpost_i","unpost_u","unpre_i","unpre_u"
);

mkdir("./usr",0755) if ( ! -d "./usr");
symlink("/etc","./etc") if ( ! -d "./etc");
symlink("/var","./var") if ( ! -d "./var");
symlink("/opt","./opt") if ( ! -d "./opt");
symlink("/usr/local","./usr/local") if ( ! -s "./usr/local");

# Open our lpp_name for writing data based on package info
open (LPP,">lpp_name");

unless ( -f "$CONFIGDIR/list" ){
  $STRING="0503-884 makebff: $CONFIGDIR/list does not exist.\n";
  $MESG=qx(/usr/bin/dspmsg -s 21 cmdinstl_e.cat 1 "$STRING");
  die "$MESG"; 
}

# Collect package info one colon-delimitted line at a time
open(LIST,"$CONFIGDIR/list");
while (<LIST>){
  $listFileLineNum++;
  chomp;                  # Get rid of the newline
  next if (/^#/ || /^$/); # Skip if we have a blank line

  @listFileLine=split(/:/,$_);
  if ($package eq ""){
    $package=$listFileLine[0];
    $packageVersion=$listFileLine[1];
    if ($listFileLine[2]){
      $packageType=$listFileLine[2];
    } else {
      $packageType="I";
    }
    printf(LPP "4 R %s %s {\n",$packageType,$package);
    $STRING="processing %s %s %s package\n";
    $MESG=qx(/usr/bin/dspmsg -s 21 cmdinstl_e.cat 2 "$STRING" "$package" "$packageVersion" "$packageType"); 
    printf "$MESG"
  } elsif (/^\*/){    # If the line starts with a '*', we have a requisite
    if ($fileset){
      $requisite{$fileset}="@listFileLine";
    } else {
      $STRING="0503-885 makebff: Requisite line for empty fileset in $CONFIGDIR/list. (%ld)\n";
      $MESG=qx(/usr/bin/dspmsg -s 21 cmdinstl_e.cat 3 "$STRING" "$listFileLineNum"); 
      die "$MESG";
    }
  } elsif (/^LAR/){   # If the line starts with 'LAR', we have a LAR
    if ($fileset){
      $lar{$fileset}="@listFileLine";
    } else {
      $STRING="0503-886 makebff: License agreement line for empty fileset in $CONFIGDIR/list. (%ld)\n";
      $MESG=qx(/usr/bin/dspmsg -s 21 cmdinstl_e.cat 4 "$STRING" "$listFileLineNum"); 
      die "$MESG";
    }
  } elsif (/^LAF/){   # If the line starts with 'LAF', we have a LAF. Ha.
    if ($fileset){
      $laf{$fileset}="@listFileLine";
    } else {
      $STRING="0503-887 makebff: License file line for empty fileset in $CONFIGDIR/list. (%ld)\n";
      $MESG=qx(/usr/bin/dspmsg -s 21 cmdinstl_e.cat 5 "$STRING" "$listFileLineNum");
      die "$MESG"; 
    }
  } else {
    # If we have a fileset name already (and it's not the name at the beginning
    # of this line), then we need to process this fileset with the data we
    # collected last time through the loop. The last fileset of the package gets
    # processed when we exit the while loop.
    if ($fileset && $fileset ne $listFileLine[0]){
      &FilesetInfo;
    }
    $fileset=$listFileLine[0];
    $version{$fileset}=$listFileLine[1];
    $description{$fileset}=$listFileLine[2];
    $bosboot{$fileset}=$listFileLine[3];

    $STRING="processing %s %s fileset\n";
    $MESG=qx(/usr/bin/dspmsg -s 21 cmdinstl_e.cat 7 "$STRING" "$fileset" "$version{$fileset}"); 
    printf "$MESG";

    # Copyrights have to go into the liblpp.a first, so keep a
    # separate array of them handy.
    if ( -f "$CONFIGDIR/$fileset.copyright"){
      push @copyrights, "$CONFIGDIR/$fileset.copyright";
    }

    if ( -f "$CONFIGDIR/$fileset.al" ){
      $hasUsrFiles=1;
      %size=&MakeInv("$CONFIGDIR/$fileset.al","$CONFIGDIR/$fileset.inventory");
    }

    # Check for ROOT part
    if ( -f "$CONFIGDIR/.create_root" ){
      $hasRoot=1;

      if ( -f "$CONFIGDIR/root/$fileset.al" ){
        $hasRootFiles=1;
        %rootSize=&MakeInv("$CONFIGDIR/root/$fileset.al","$CONFIGDIR/root/$fileset.inventory");
      }
    }
  }
}
&FilesetInfo;
printf(LPP "}\n");
close(LPP);
close(LIST);

mkdir("./usr/lpp",0755) if ( ! -d "./usr/lpp");
mkdir("./usr/lpp/$package",0755) if ( ! -d "./usr/lpp/$package");

# create big liblpp.a for install packages, and fileset liblpp.a's for update packages
if ($packageType eq "I"){
  $STRING="creating ./usr/lpp/$package/liblpp.a\n";
  $MESG=qx(/usr/bin/dspmsg -s 21 cmdinstl_e.cat 9 "$STRING"); 
  printf "$MESG";
  unlink("./usr/lpp/$package/liblpp.a");
  system("ar -crlg ./usr/lpp/$package/liblpp.a @copyrights @usrFilesForArchive");

  if ( $hasRoot ){
    mkdir("./usr/lpp/$package/inst_root",0755) if ( ! -d "./usr/lpp/$package/inst_root");
    unlink("./usr/lpp/$package/inst_root/liblpp.a");
    system("ar -crlg ./usr/lpp/$package/inst_root/liblpp.a @rootFilesForArchive");
  }
} else {
  # The update fileset.a files are created in sub FilesetInfo
  foreach my $fileset (keys %version){
    $filesetDir=sprintf("./usr/lpp/%s/%s",$package,$fileset);
    mkdir($filesetDir,0755) if ( ! -d $filesetDir);
    push @updateLibDirs, $filesetDir;

    $vrmfDir=sprintf("./usr/lpp/%s/%s/%s",$package,$fileset,$version{$fileset});
    mkdir($vrmfDir,0755) if ( ! -d $vrmfDir);
    push @updateLibDirs, $vrmfDir;

    $STRING="copying $CONFIGDIR/%s.a to %s/liblpp.a\n";
    $MESG=qx(/usr/bin/dspmsg -s 21 cmdinstl_e.cat 10 "$STRING" "$fileset" "$vrmfDir");
    printf "$MESG";
    system("cp $CONFIGDIR/$fileset.a $vrmfDir/liblpp.a");
    push @liblpp, "$vrmfDir/liblpp.a";

    if ($hasRoot){
      $instrootDir=sprintf("./usr/lpp/%s/%s/%s/inst_root",$package,$fileset,$version{$fileset});
      mkdir($instrootDir,0755) if ( ! -d $instrootDir);
      push @updateLibDirs, $instrootDir;

      system("cp $CONFIGDIR/root/$fileset.a $instrootDir/liblpp.a");
      push @liblpp, "$instrootDir/liblpp.a";
    }
  }
}

mkdir("./tmp",0755) if ( ! -d "./tmp");
$STRING="creating ./tmp/%s.%s.bff\n";
$MESG=qx(/usr/bin/dspmsg -s 21 cmdinstl_e.cat 11 "$STRING" "$package" "$packageVersion");
printf "$MESG"; 

# ./usr/lpp/<pkg> needs to immediately follow lpp_name
open(TT,"|backup -irqpf ./tmp/$package.$packageVersion.bff -b1");
print TT "./\n";
print TT "./lpp_name\n";
print TT "./usr\n";
print TT "./usr/lpp\n";
print TT "./usr/lpp/$package\n";

if ($packageType eq "I"){
  print TT "./usr/lpp/$package/liblpp.a\n";
  if ( $hasRoot ){
    print TT "./usr/lpp/$package/inst_root\n";
    print TT "./usr/lpp/$package/inst_root/liblpp.a\n";
  }
} else {
  # Backup all the libdirs (may be multiple for update packages)
  foreach my $libDir (@updateLibDirs){
    print TT "$libDir\n";
  }

  # Backup all liblpp.a files (may be multiple for update packages)
  foreach my $archive (@liblpp){
    print TT "$archive\n";
  }
}

# Backup all files in each fileset's .al, and the LAFs if they exist
foreach my $fileset (keys %version){
  if ( $hasUsrFiles ){
    my $applyListFile="$CONFIGDIR/$fileset.al";
    open(AL,"<$applyListFile");
    while (<AL>){
      print TT $_;
    }
    close(AL);
  }

  if ( $hasRootFiles ){
    my $rootAL="$CONFIGDIR/root/$fileset.al";
    open(RAL,"<$rootAL");
    while (<RAL>){
      print TT $_;
    }
    close(RAL);
  }

  # Put LAFs in the backup
  if ($laf{$fileset}) {
    my @lafs=split (/;/, $laf{$fileset});
    foreach my $laf (@lafs) {
      $laf =~ s/^LAF//g;
      $laf =~ s/^<.._..>//;
      print TT "." . $laf . "\n";
    }
  }
}

close(TT);
exit(0);


# Gather fileset-specific info. Called for each fileset in this package.
sub FilesetInfo {
  # some fileset-specific vars
  my $extraSizeFile="";
  my @lafs=();
  my @reqs=();
  my $rootSizeFile="";
  my @rootConfigFiles=();
  my $usrSizeFile="";
  my @usrConfigFiles=();
  my $totalSize=0;

  # stat output vars
  my ($dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size,
         $atime, $mtime, $ctime, $blksize, $blocks);

  $usrSizeFile   = "$CONFIGDIR/$fileset.size";
  $rootSizeFile  = "$CONFIGDIR/root/$fileset.size" if ( $hasRootFiles );
  $extraSizeFile = "$CONFIGDIR/$fileset.upsize" if ( -f "$CONFIGDIR/$fileset.upsize");
  $extraSizeFile = "$CONFIGDIR/$fileset.insize" if ( -f "$CONFIGDIR/$fileset.insize");
  
  # Print fileset info to lpp_name. Set correct header if we have a ROOT part.
  if ( $hasRoot ){
    printf(LPP "%s %s 01 %s B en_US %s\n",
      $fileset,$version{$fileset},$bosboot{$fileset},$description{$fileset});
  } else {
    printf(LPP "%s %s 01 %s U en_US %s\n",
      $fileset,$version{$fileset},$bosboot{$fileset},$description{$fileset});
  }
  printf(LPP "[\n");

  # Print reqs on separate lines in lpp_name (if we have any)
  if ($requisite{$fileset}){
    # If the line starts with "*." read in the lines of the file
    if ($requisite{$fileset} =~ m/^\*\./) {
       my $req = $requisite{$fileset};
       $req =~ s/^\*//;
       open(RF,"<$req");
       while (<RF>){
         print LPP $_;
       }
       close(RF);
    } else {
       # Else process the requisite list
       @reqs=split (/;/, $requisite{$fileset});
       foreach my $req (@reqs){
         printf(LPP "%s\n",$req);
       }
    }
  }
  printf(LPP "%%\n");

  # Open our fileset.size file for writing (data is dup'd in lpp_name).
  open(SF,">$usrSizeFile");
  if ( $hasRootFiles ){
    open(RSF,">$rootSizeFile");
  }

  # If we don't have an extra size file, just use the size data stored in %size.
  # We have to put this data in the lpp_name AND fileset.size file.
  if ($extraSizeFile eq ""){
    foreach my $dir (sort keys %size){
      printf(LPP "%s %d\n",$dir,$size{$dir});
      printf(SF "%s %d\n",$dir,$size{$dir});
      $totalSize+=$size{$dir};
    }
  }
  # Otherwise, include our extra size file's data
  else{
    my @extraSizeFileEntryParts=();
    my $extraSizeFileEntryPath="";
    my $extraSizeFileEntryPathNotFound=1;
    my $extraSizeFileEntrySize=0;

    foreach my $dir (sort keys %size){
      open (FUSIZE, $extraSizeFile);
      while (<FUSIZE>){
        $extraSizeFileEntryPathNotFound = 1;
        @extraSizeFileEntryParts=split(/ /, $_);
        $extraSizeFileEntryPath=$extraSizeFileEntryParts[0];
        $extraSizeFileEntrySize=$extraSizeFileEntryParts[1];

        if ($dir eq "$extraSizeFileEntryPath"){
          printf(LPP "%s %d\n",$dir,$size{$dir}+$extraSizeFileEntrySize);
          printf(SF "%s %d\n",$dir,$size{$dir}+$extraSizeFileEntrySize);
          $totalSize+=$size{$dir};
          $extraSizeFileEntryPathNotFound = 0;
          last;
        }
      }
      close (FUSIZE);
      if ($extraSizeFileEntryPathNotFound){ 
        printf(LPP "%s %d\n",$dir,$size{$dir});
        printf(SF "%s %d\n",$dir,$size{$dir});
        $totalSize+=$size{$dir};
      }
    }

    open (FUSIZE, $extraSizeFile);
    while (<FUSIZE>){
      @extraSizeFileEntryParts=split(/ /, $_);
      $extraSizeFileEntryPath=$extraSizeFileEntryParts[0];
      $extraSizeFileEntryPathNotFound = 1;
      $extraSizeFileEntrySize=$extraSizeFileEntryParts[1];

      foreach my $dir (sort keys %size){
        if ($dir eq "$extraSizeFileEntryPath"){
          $extraSizeFileEntryPathNotFound = 0;
        }
      }
      if ($extraSizeFileEntryPathNotFound){
        printf(LPP "%s %d\n", "$extraSizeFileEntryPath", $extraSizeFileEntrySize);
        printf(SF "%s %d\n", "$extraSizeFileEntryPath", $extraSizeFileEntrySize);
        $totalSize+=$extraSizeFileEntrySize;
      }
    } 
    close (FUSIZE);
  }

  # Gather ROOT part size info (there's not insize/upsize for ROOT parts)
  if ( $hasRootFiles ){
    foreach my $dir (sort keys %rootSize){
      printf(LPP "%s %d\n",$dir,$rootSize{$dir});
      printf(RSF "%s %d\n",$dir,$rootSize{$dir});
      $totalSize+=$rootSize{$dir};
    }
  }

  # Gather USR part size info for each existing config file
  foreach my $ext (@configFiles){
    my $configFile="";

    if ($ext eq "README"){
      $configFile="$CONFIGDIR/lpp.README";
    } elsif ($ext eq "productid") {
      $configFile="$CONFIGDIR/productid";
    } else {
      $configFile="$CONFIGDIR/$fileset.$ext";
    }
    if ( -f $configFile ){
      ($dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size,
       $atime, $mtime, $ctime, $blksize, $blocks) = stat "$configFile";
      $configFileBlocks+=$blocks;

      # Dont push the copyright on - it has to be first in the liblpp.a
      next if ($ext eq "copyright");
      push @usrConfigFiles, $configFile;
    }
  }

  # Gather ROOT part size info for root config files
  if ( $hasRoot ){
    # Gather size info for each existing config file
    foreach my $ext (@configFiles){
      my $configFile="";
      $configFile="$CONFIGDIR/root/$fileset.$ext";

      if ( -f $configFile ){
        ($dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size,
         $atime, $mtime, $ctime, $blksize, $blocks) = stat "$configFile";
        $configFileBlocks+=$blocks;

        push @rootConfigFiles, $configFile;
      }
    }
  }

  # Archive the config files
  if ($packageType=~/^S/){
    # SAVESPACE is what's needed if this update is removed
    printf(LPP "/usr/lpp/SAVESPACE %d\n",$totalSize);
    printf(SF "/usr/lpp/SAVESPACE %d\n",$totalSize);

    $STRING="creating $CONFIGDIR/%s.a\n";
    $MESG=qx(/usr/bin/dspmsg -s 21 cmdinstl_e.cat 6 "$STRING" "$fileset");
    printf "$MESG";
    unlink("$CONFIGDIR/$fileset.a");
    system("ar -crlg $CONFIGDIR/$fileset.a @usrConfigFiles");
    if ( $hasRoot ){
      unlink("$CONFIGDIR/root/$fileset.a");
      system("ar -crlg $CONFIGDIR/root/$fileset.a @rootConfigFiles");
    }
  } else {
    # All of this fileset's config files will be archived with
    # other filsets' config files (if they exist)
    push @usrFilesForArchive, @usrConfigFiles;

    if ( $hasRoot ){
      push @rootFilesForArchive, @rootConfigFiles;
    }
  }

  # Assuming 1 block per file for objrepos, and equal perm/temp space for INSTWORK
  if ( $hasRoot ){
    printf(LPP "/etc/objrepos $numFiles\n");

    if ($hasRootFiles) {
      printf(RSF "/etc/objrepos $numFiles\n");
      printf(RSF "INSTWORK $configFileBlocks $configFileBlocks\n");
    }
  }
  printf(LPP "/usr/lib/objrepos $numFiles\n");
  printf(LPP "INSTWORK $configFileBlocks $configFileBlocks\n");
  printf(SF "/usr/lib/objrepos $numFiles\n");
  printf(SF "INSTWORK $configFileBlocks $configFileBlocks\n");

  # Put LAFs/LAR in lpp_name
  if ($laf{$fileset}) {
    @lafs=split (/;/, $laf{$fileset});
    foreach my $laf (@lafs) {
      my $lafString = $laf;
      $laf =~ s/^LAF//g;
      $laf =~ s/^<.._..>//;
      if ( -f ".$laf") {
        ($dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size,
         $atime, $mtime, $ctime, $blksize, $blocks) = stat ".$laf";

        printf(LPP "%s %d\n", $lafString, $blocks);
      }
    }
  }
  if ($lar{$fileset}) {
    printf(LPP "%s 0\n", $lar{$fileset});
  }

  printf(LPP "%%\n");

  # Put supersede info in lpp_name
  if ( -f "$CONFIGDIR/$fileset.supersede") {
    open (SUPER, "<$CONFIGDIR/$fileset.supersede");
    while (<SUPER>) {
      printf(LPP $_);
    }
    close (SUPER);
  }
  printf(LPP "%%\n");
  printf(LPP "%%\n");
  printf(LPP "]\n");
  close (SF);
  if ( $hasRootFiles ){
    close (RSF);
  }
}


# Gather file-specific data and create .inventory for each fileset in this package.
sub MakeInv {
  # The .al and .inventory filenames are passed in
  my ($al, $inv)=@_;

  my $dir="";          # Dir name for keeping track of dir size reqs
  my %dirSize=();      # Map associating directories and their size
  my $fileName="";     # Filename for .inventory
  my %gname=();        # Map group names/gids
  my @ilist=();        # Stores files to be listed in the .inventory
  my $key="";          # Key for referencing files
  my $passwd="";       # Needed for getgrent
  my $sum="";          # Output of the sum command
  my $sysname="";      # Needed for getgrent
  my $type="";         # File/Link/Dir
  my %uname=();        # Map user names/uids

  # stat output vars
  my ($dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size,
         $atime, $mtime, $ctime, $blksize, $blocks);

  # .inventory related
  my (%name, %owner, %group, %mode, %type, %class, %size, %checksum);

  # link related
  my (%hardlinkList, %nlink, %target);

  # Maps associating files and their attributes
  %name=();
  %owner=();
  %group=();
  %mode=();
  %type=();
  %class=();
  %size=();
  %checksum=();
  %hardlinkList=();    # List of hard links to a file
  %nlink=();           # Number of hard links to a file
  %target=();          # Symlink target

  # Create maps of gids to groups and uids to users
  while (($sysname, $passwd, $gid) = getgrent){
    next if ($sysname =~ /^\+/); # skip NIS password lines (begin with plus sign)
    $gname{$gid}=$sysname;
  }
  endgrent();
  while (($sysname, $passwd, $uid) = getpwent){
    next if ($sysname =~ /^\+/); # skip NIS password lines (begin with plus sign)
    $uname{$uid}=$sysname;
  }
  endpwent();

  # Ensure a uid/gid of 0 is root/system
  $gname{0}="system";
  $uname{0}="root";

  open(AL,"<$al");
  open(INV,">$inv");

  unlink("$al.withoutlinks");
  open(ALWOL,">$al.withoutlinks"); # Need to strip hard links out of the applylist
  while (<AL>){
    chomp;
    $dir=$_;
    if (-l){
      $type="SYMLINK";
      $dir=~s?/[^/]*$??;
    } elsif (-d){
      $type="DIRECTORY";
    } elsif (-f){
      $type="FILE";
      $dir=~s?/[^/]*$??;
    } else {
      $STRING="no such file: %s\n";
      $MESG=qx(/usr/bin/dspmsg -s 21 cmdinstl_e.cat 8 "$STRING" "$_");
      printf(STDERR "$MESG");
      next;
    }

    # Increment our file count
    $numFiles++;

    ($dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size,
     $atime, $mtime, $ctime, $blksize, $blocks) = lstat "$_";
    if ($type eq "SYMLINK"){
      $key=$symlinkKey++;
    } else {
      $key="$dev.$ino";
    }

    # If we have this key already, then make a hardlink list
    if ($name{$key} && ($nlink > 1) && ($type eq "FILE")){
      if ($hardlinkList{$key}){
        $hardlinkList{$key} .= "," . substr $_, 1;
      } else {
        $hardlinkList{$key} = substr $_, 1;
      }
    } else {
      printf(ALWOL "%s\n", $_);
      push(@ilist,$key);
      $name{$key}=$_;
      $owner{$key}=$uname{$uid};
      $group{$key}=$gname{$gid};
      $mode{$key}=$mode;
      $type{$key}=$type;
      $class{$key}="apply,inventory,$fileset";
      $size{$key}=$size;
      if ($type eq "FILE"){
        $sum=`/usr/bin/sum $_`;
        $sum=~s/ +/ /g;
        ($a, $b)=(split(/ /,$sum))[0,1];
        $checksum{$key}=sprintf("\"%s    %s \"",$a,$b);
      } elsif ($type eq "SYMLINK"){
        $target{$key}=readlink($_);
      }
      $hardlinkList{$key}="";
      $dirSize{substr $dir, 1}+=$blocks;
    }
  }

  # Print out the inventory file; spacing and order is done 
  # on purpose to look like BOS install images.
  foreach my $key (@ilist){
    $fileName=$name{$key};
    $fileName=~s/^.//;
    printf(INV "%s:\n",$fileName);
    if ($type{$key} eq "FILE" || $type{$key} eq "DIRECTORY"){
      printf(INV "          owner = %s\n",$owner{$key});
      printf(INV "          group = %s\n",$group{$key});
      printf(INV "          mode = %o\n",$mode{$key} & 07777);
    }
    printf(INV "          type = %s\n",$type{$key});
    if ($hardlinkList{$key}){
      printf(INV "          links = %s\n",$hardlinkList{$key});
    }
    printf(INV "          class = %s\n",$class{$key});
    if ($type{$key} eq "FILE"){
      printf(INV "          size = %d\n",$size{$key});
      printf(INV "          checksum = %s\n",$checksum{$key});
    } elsif ($type{$key} eq "SYMLINK"){
      printf(INV "          target = %s\n",$target{$key});
    }
    printf(INV "\n");
  }
  close(AL);
  close(ALWOL);
  close(INV);

  # Get rid of our original apply list in favor of the non-hardlink'd one
  unlink($al);
  rename("$al.withoutlinks", $al);

  return %dirSize;
}

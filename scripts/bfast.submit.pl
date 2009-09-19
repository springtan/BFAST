#!/usr/bin/perl

# Please see the LICENSE accompanying this distribution for 
# details.  Report all bugs to nhomer@cs.ucla.edu or 
# bfast-help@lists.sourceforge.net.  For documentation, use the
# -man option.

use strict;
use warnings;
use File::Path;
use XML::Simple; 
use Data::Dumper;
use Getopt::Long;
use Pod::Usage;
use File::Path; # for directory creation
use Cwd;

# TODO:
# - input values could be recognizable strings, such as 10GB for maxMemory...

my %QUEUETYPES = ("SGE" => 0, "PBS" => 1);
my %SPACE = ("NT" => 0, "CS" => 1);
my %TIMING = ("ON" => 1);
my %LOCALALIGNMENTTYPE = ("GAPPED" => 0, "UNGAPPED" => 1);
my %STRAND = ("BOTH" => 0, "FORWARD" => 1, "REVERSE" => 2);

use constant {
	OPTIONAL => 0,
	REQUIRED => 1,
	BREAKLINE => "************************************************************\n",
	MERGE_LOG_BASE => 8,
};

my $config;
my ($man, $print_schema, $help, $quiet) = (0, 0, 0, 0);
my $version = "0.1.1";

GetOptions('help|?' => \$help, man => \$man, 'schema' => \$print_schema, 'quiet' => \$quiet, 'config=s' => \$config) or pod2usage(1);
Schema() if ($print_schema);
pod2usage(-exitstatus => 0, -verbose => 2) if $man;
pod2usage(1) if ($help or !defined($config));

if(!$quiet) {
	print STDOUT BREAKLINE;
}

# Read in from the config file
my $xml = new XML::Simple;
my $data = $xml->XMLin("$config");

# Validate data
ValidateData($data);

# Submit jobs
CreateJobs($data, $quiet);

if(!$quiet) {
	print STDOUT BREAKLINE;
}

sub Schema {
	# print the schema
	my $schema = <<END;
<?xml version="1.0"?>
<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:element name="bfastConfig">
	<xs:complexType>
	  <xs:sequence>
		<xs:element name="globalOptions">
		  <xs:complexType>
			<xs:sequence>
			  <xs:element name="bfastBin" type="directoryPath"/>
			  <xs:element name="samtoolsBin" type="directoryPath"/>
			  <xs:element name="qsubBin" type="directoryPath"/>
			  <xs:element name="referenceFasta" type="filePath" use="required"/>
			  <xs:element name="brgNT" type="filePath" use="required"/>
			  <xs:element name="brgCS" type="filePath"/>
			  <xs:element name="runDirectory" type="directoryPath" use="required">
			  <xs:element name="readsDirectory" type="directoryPath" use="required"/>
			  <xs:element name="outputDirectory" type="directoryPath" use="required"/>
			  <xs:element name="tmpDirectory" type="directoryPath" use="required"/>
			  <xs:element name="outputID" type="xs:string" use="required"/>
			  <xs:element name="numReadsPerFASTQ" type="positiveInteger">
				<xs:complexType>
				  <xs:attribute name="bmatchesSplit" type="positiveInteger" use="required"/>
				  <xs:attribute name="balignSplit" type="positiveInteger" use="required"/>
				</xs:complexType>
			  </xs:element>
			  <xs:element name="timing">
				<xs:simpleType>
				  <xs:restriction base="xs:string">
					<xs:enumeration value="ON"/>
				  </xs:restriction>
				</xs:simpleType>
			  </xs:element>
			  <xs:element name="queueType" use="required">
				<xs:simpleType>
				  <xs:restriction base="xs:string">
					<xs:enumeration value="SGE"/>
					<xs:enumeration value="PBS"/>
				  </xs:restriction>
				</xs:simpleType>
			  </xs:element>
			  <xs:element name="space" use="required">
				<xs:simpleType>
				  <xs:restriction base="xs:string">
					<xs:enumeration value="NT"/>
					<xs:enumeration value="CS"/>
				  </xs:restriction>
				</xs:simpleType>
			  </xs:element>
			</xs:sequence>
		  </xs:complexType>
		</xs:element>
		<xs:element name="bmatchesOptions">
		  <xs:complexType>
			<xs:sequence>
			  <xs:element name="mainIndexes" type="filePath" use="required"/>
			  <xs:element name="secondaryIndexes" type="filePath"/>
			  <xs:element name="offsets" type="filePath"/>
			  <xs:element name="keySize" type="xs:positiveInteger"/>
			  <xs:element name="maxKeyMatches" type="xs:positiveInteger"/>
			  <xs:element name="maxNumMatches" type="xs:positiveInteger"/>
			  <xs:element name="strand">
				<xs:simpleType>
				  <xs:restriction base="xs:string">
					<xs:enumeration value="BOTH"/>
					<xs:enumeration value="FORWARD"/>
					<xs:enumeration value="REVERSE"/>
				  </xs:restriction>
				</xs:simpleType>
			  </xs:element>
			  <xs:element name="threads" type="xs:positiveInteger"/>
			  <xs:element name="queueLength" type="xs:positiveInteger"/>
			  <xs:element name="qsubQueue" type="xs:string"/>
			</xs:sequence>
		  </xs:complexType>
		</xs:element>
		<xs:element name="balignOptions">
		  <xs:complexType>
			<xs:sequence>
			  <xs:element name="scoringMatrix" type="filePath"/>
			  <xs:element name="localAlignmentType">
				<xs:simpleType>
				  <xs:restriction base="xs:integer">
					<xs:enumeration value="0"/>
					<xs:enumeration value="1"/>
				  </xs:restriction>
				</xs:simpleType>
			  </xs:element>
			  <xs:element name="offset" type="xs:nonNegativeInteger"/>
			  <xs:element name="maxNumMatches" type="xs:positiveInteger"/>
			  <xs:element name="mismatchQuality" type="xs:positiveInteger"/>
			  <xs:element name="threads" type="xs:positiveInteger"/>
			  <xs:element name="queueLength" type="xs:positiveInteger"/>
			  <xs:element name="pairedEndLength" type="xs:integer"/>
			  <xs:element name="mirrorType" type="xs:integer"/>
			  <xs:element name="forceMirror">
				<xs:simpleType>
				  <xs:restriction base="xs:integer">
					<xs:minInclusive value="0"/>
					<xs:maxInclusive value="3"/>
				  </xs:restriction>
				</xs:simpleType>
			  </xs:element>
			  <xs:element name="qsubQueue" type="xs:string"/>
			</xs:sequence>
		  </xs:complexType>
		</xs:element>
		<xs:element name="bpostprocessOptions">
		  <xs:complexType>
			<xs:sequence>
			  <xs:element name="algorithm">
				<xs:simpleType>
				  <xs:restriction base="xs:integer">
					<xs:minInclusive value="0"/>
					<xs:maxInclusive value="3"/>
				  </xs:restriction>
				</xs:simpleType>
			  </xs:element>
			  <xs:element name="queueLength" type="xs:positiveInteger"/>
			  <xs:element name="qsubQueue" type="xs:string"/>
			</xs:sequence>
		  </xs:complexType>
		</xs:element>
		<xs:element name="samtoolsOptions">
		  <xs:complexType>
			<xs:sequence>
			  <xs:element name="maximumMemory" type="xs:positiveInteger"/>
			  <xs:element name="qsubQueue" type="xs:string"/>
			</xs:sequence>
		  </xs:complexType>
		</xs:element>
	  </xs:sequence>
	</xs:complexType>
  </xs:element>
  <xs:simpleType name="filePath">
	<xs:restriction base="xs:string">
	  <xs:pattern value="\\S+"/>
	</xs:restriction>
  </xs:simpleType>
  <xs:simpleType name="directoryPath">
	<xs:restriction base="xs:string">
	  <xs:pattern value="\\S+/"/>
	</xs:restriction>
  </xs:simpleType>
  <xs:simpleType name="nonNegativeInteger">
	<xs:restriction base="xs:integer">
	  <xs:minInclusive value="0"/>
	</xs:restriction>
  </xs:simpleType>
  <xs:simpleType name="positiveInteger">
	<xs:restriction base="xs:integer">
	  <xs:minInclusive value="1"/>
	</xs:restriction>
  </xs:simpleType>
</xs:schema>
END
	print STDOUT $schema;
	exit 1;
}

sub ValidateData {
	my $data = shift;

	# global options
	die("The global options were not found.\n") unless (defined($data->{'globalOptions'})); 
	ValidatePath($data->{'globalOptions'},         'bfastBin',                                 OPTIONAL); 
	ValidatePath($data->{'globalOptions'},         'samtoolsBin',                              OPTIONAL); 
	ValidatePath($data->{'globalOptions'},         'qsubBin',                                  OPTIONAL); 
	ValidateOptions($data->{'globalOptions'},      'queueType',          \%QUEUETYPES,         REQUIRED);
	ValidateOptions($data->{'globalOptions'},      'space',              \%SPACE,              REQUIRED);
	ValidateFile($data->{'globalOptions'},         'referenceFasta',                           REQUIRED);
	ValidateFile($data->{'globalOptions'},         'brgNT',                                    REQUIRED);
	ValidateFile($data->{'globalOptions'},         'brgCS',                                    REQUIRED) if ("CS" eq $data->{'globalOptions'}->{'space'});
	ValidateOptions($data->{'globalOptions'},      'timing',             \%TIMING,             OPTIONAL);
	ValidatePath($data->{'globalOptions'},         'runDirectory',                             REQUIRED); 
	ValidatePath($data->{'globalOptions'},         'readsDirectory',                           REQUIRED); 
	ValidatePath($data->{'globalOptions'},         'outputDirectory',                          REQUIRED); 
	ValidatePath($data->{'globalOptions'},         'tmpDirectory',                             REQUIRED); 
	ValidateOption($data->{'globalOptions'},       'outputID',                                 REQUIRED); 
	ValidateOption($data->{'globalOptions'},       'numReadsPerFASTQ',                         REQUIRED);
	die "Attribute bmatchesSplit required with numReadsPerFASTQ\n" if (!defined($data->{'globalOptions'}->{'numReadsPerFASTQ'}->{'bmatchesSplit'}));
	die "Attribute bmatchesSplit required with numReadsPerFASTQ\n" if (!defined($data->{'globalOptions'}->{'numReadsPerFASTQ'}->{'balignSplit'}));
	die "Attribute bmatchesSplit must be <= numReadsPerFASTQ\n" if ($data->{'globalOptions'}->{'numReadsPerFASTQ'}->{'content'} < $data->{'globalOptions'}->{'numReadsPerFASTQ'}->{'bmatchesSplit'});
	die "Attribute balignSplit must be <= bmatchesSplit\n" if ($data->{'globalOptions'}->{'numReadsPerFASTQ'}->{'bmatchesSplit'} < $data->{'globalOptions'}->{'numReadsPerFASTQ'}->{'balignSplit'});

	# bmatches
	die("The bmatches options were not found.\n") unless (defined($data->{'bmatchesOptions'})); 
	ValidateFile($data->{'bmatchesOptions'},       'mainIndexes',                              REQUIRED);
	ValidateFile($data->{'bmatchesOptions'},       'secondaryIndexes',                         OPTIONAL);
	ValidateFile($data->{'bmatchesOptions'},       'offsets',                                  OPTIONAL);
	ValidateOption($data->{'bmatchesOptions'},     'keySize',                                  OPTIONAL);
	ValidateOption($data->{'bmatchesOptions'},     'maxKeyMatches',                            OPTIONAL);
	ValidateOption($data->{'bmatchesOptions'},     'maxNumMatches',                            OPTIONAL);
	ValidateOptions($data->{'bmatchesOptions'},    'strand',             \%STRAND,             OPTIONAL);
	ValidateOption($data->{'bmatchesOptions'},     'threads',                                  OPTIONAL);
	ValidateOption($data->{'bmatchesOptions'},     'queueLength',                              OPTIONAL);
	ValidateOption($data->{'bmatchesOptions'},     'qsubQueue',                                OPTIONAL);

	# balign
	die("The balign options were not found.\n") unless (defined($data->{'balignOptions'})); 
	ValidateFile($data->{'balignOptions'},         'scoringMatrix',                            OPTIONAL);
	ValidateOptions($data->{'balignOptions'},      'localAlignmentType', \%LOCALALIGNMENTTYPE, OPTIONAL);
	ValidateFile($data->{'balignOptions'},         'offset',                                   OPTIONAL);
	ValidateOption($data->{'balignOptions'},       'maxNumMatches',                            OPTIONAL);
	ValidateOption($data->{'balignOptions'},       'mismatchQuality',                          OPTIONAL);
	ValidateOption($data->{'balignOptions'},       'queueLength',                              OPTIONAL);
	ValidateOption($data->{'balignOptions'},       'pairedEndLength',                          OPTIONAL);
	ValidateOption($data->{'balignOptions'},       'mirrorType',                               OPTIONAL);
	ValidateOption($data->{'balignOptions'},       'forceMirror',                              OPTIONAL);
	ValidateOption($data->{'balignOptions'},       'threads',                                  OPTIONAL);
	ValidateOption($data->{'balignOptions'},       'qsubQueue',                                OPTIONAL);

	# bpostprocess
	die("The bpostprocess options were not found.\n") unless (defined($data->{'bpostprocessOptions'})); 
	ValidateOption($data->{'bpostprocessOptions'}, 'algorithm',                                OPTIONAL);
	ValidateOption($data->{'bpostprocessOptions'}, 'queueLength',                              OPTIONAL);
	ValidateOption($data->{'bpostprocessOptions'}, 'qsubQueue',                                OPTIONAL);

	# samtools
	die("The samtools options were not found.\n") unless (defined($data->{'samtoolsOptions'})); 
	ValidateOption($data->{'samtoolsOptions'},     'maximumMemory',                            OPTIONAL);
	ValidateOption($data->{'samtoolsOptions'},     'qsubQueue',                                OPTIONAL);
}

sub ValidateOption {
	my ($hash, $option, $required) = @_;

	return 1 if (defined($hash->{$option}));
	return 0 if (OPTIONAL == $required);

	die("Option '$option' was not found.\n");
}

sub ValidatePath {
	my ($hash, $option, $required) = @_; 

	if(0 != ValidateOption($hash, $option, $required) and $hash->{$option} !~ m/\S+\//) { # very liberal check
		die("Option '$option' did not give a valid path.\n");
	}
}

sub ValidateFile {
	my ($hash, $option, $required) = @_;

	if(0 != ValidateOption($hash, $option, $required) and $hash->{$option} !~ m/\S+/) { # very liberal check
		die("Option '$option' did not give a valid file name.\n");
	}
}

sub ValidateOptions {
	my ($hash, $option, $values, $required) = @_;

	if(0 != ValidateOption($hash, $option, $required) and !defined($values->{$hash->{$option}})) {
		die("The value '".($hash->{$option})."' for option '$option' was not valid.\n");
	}
}

sub GetDirContents {
	my ($dir, $dirs, $suffix) = @_;

	if(!($dir =~ m/\/$/)) {
		$dir .= "/";
	}

	local *DIR;
	opendir(DIR, "$dir") or die("Error.  Could not open $dir.  Terminating!\n");
	@$dirs = grep !/^\.\.?$/, readdir DIR;
	for(my $i=0;$i<scalar(@$dirs);$i++) {
		@$dirs[$i] = $dir."".@$dirs[$i];
	}
	close(DIR);

	@$dirs= grep /\.$suffix$/, @$dirs;
	if(0 == scalar(@$dirs)) {
		die("Did not find any '$suffix' files\n");;
	}

	@$dirs = sort { $a cmp $b } @$dirs;
}

sub CreateJobs {
	my ($data, $quiet) = @_;

	my @bmatches_output_ids = ();
	my @balign_output_ids = ();
	my @bmatchesJobIDs = ();
	my @balignJobIDs = ();
	my @bpostprocessJobIDs = ();

	# Create directories - Error checking...
	mkpath([$data->{'globalOptions'}->{'runDirectory'}],    ($quiet) ? 0 : 1, 0755);
	mkpath([$data->{'globalOptions'}->{'outputDirectory'}], ($quiet) ? 0 : 1, 0755);
	mkpath([$data->{'globalOptions'}->{'tmpDirectory'}],    ($quiet) ? 0 : 1, 0755);

	# bmatches
	CreateJobsBmatches($data,     $quiet, \@bmatchesJobIDs,     \@bmatches_output_ids);
	CreateJobsBalign($data,       $quiet, \@bmatchesJobIDs,     \@balignJobIDs,       \@bmatches_output_ids, \@balign_output_ids);
	CreateJobsBpostprocess($data, $quiet, \@balignJobIDs,       \@bpostprocessJobIDs, \@balign_output_ids);
	CreateJobsSamtools($data,     $quiet, \@bpostprocessJobIDs, \@balign_output_ids);
}

sub CreateRunFile {
	my ($data, $type, $output_id) = @_;

	return $data->{'globalOptions'}->{'runDirectory'}."$type.$output_id.sh";
}

sub GetMatchesFile {
	my ($data, $output_id) = @_;

	return sprintf("%sbfast.matches.file.%s.bmf",
		$data->{'globalOptions'}->{'outputDirectory'},
		$output_id);
}

sub GetAlignFile {
	my ($data, $output_id) = @_;

	return sprintf("%sbfast.aligned.file.%s.baf",
		$data->{'globalOptions'}->{'outputDirectory'},
		$output_id);
}

sub GetReportedFile {
	my ($data, $output_id) = @_;

	return sprintf("%sbfast.reported.file.%s.sam",
		$data->{'globalOptions'}->{'outputDirectory'},
		$output_id);
}

sub CreateJobsBmatches {
	my ($data, $quiet, $qsub_ids, $output_ids) = @_;
	my @read_files = ();

	# Get reads
	GetDirContents($data->{'globalOptions'}->{'readsDirectory'}, \@read_files, 'fastq');

	# The number of bmatches to perform per read file
	my $num_split_files = int(0.5 + ($data->{'globalOptions'}->{'numReadsPerFASTQ'}->{'content'}/$data->{'globalOptions'}->{'numReadsPerFASTQ'}->{'bmatchesSplit'}));

	# Go through each
	foreach my $read_file (@read_files) {
		my ($cur_read_num_start, $cur_read_num_end) = (1, $data->{'globalOptions'}->{'numReadsPerFASTQ'}->{'bmatchesSplit'}); 
		for(my $i=0;$i<$num_split_files;$i++) {
			my $output_id = $read_file; 
			$output_id =~ s/.*?([^\/]+\d+)\.[^\.]+$/$1/; 
			$output_id = $data->{'globalOptions'}->{'outputID'}.".$output_id.reads.$cur_read_num_start-$cur_read_num_end";
			my $run_file = CreateRunFile($data, 'bmatches', $output_id);
			my $cmd = "";
			$cmd .= $data->{'globalOptions'}->{'bfastBin'}."bmatches/"      if defined($data->{'globalOptions'}->{'bfastBin'});
			$cmd .= "bmatches";
			$cmd .= " -r ".$data->{'globalOptions'}->{'brgNT'}              if ("NT" eq $data->{'globalOptions'}->{'space'});
			$cmd .= " -r ".$data->{'globalOptions'}->{'brgCS'}              if ("CS" eq $data->{'globalOptions'}->{'space'});
			$cmd .= " -i ".$data->{'bmatchesOptions'}->{'mainIndexes'};
			$cmd .= " -I ".$data->{'bmatchesOptions'}->{'secondaryIndexes'} if defined($data->{'bmatchesOptions'}->{'secondaryIndexes'});
			$cmd .= " -R ".$read_file;
			$cmd .= " -O ".$data->{'bmatchesOptions'}->{'offsets'}          if defined($data->{'bmatchesOptions'}->{'offsets'});
			$cmd .= " -A 1"                                                 if ("CS" eq $data->{'globalOptions'}->{'space'});
			$cmd .= " -s $cur_read_num_start -e $cur_read_num_end";
			$cmd .= " -k ".$data->{'bmatchesOptions'}->{'keySize'}          if defined($data->{'bmatchesOptions'}->{'keySize'});
			$cmd .= " -K ".$data->{'bmatchesOptions'}->{'maxKeyMatches'}    if defined($data->{'bmatchesOptions'}->{'maxKeyMatches'});
			$cmd .= " -M ".$data->{'bmatchesOptions'}->{'maxNumMatches'}    if defined($data->{'bmatchesOptions'}->{'maxNumMatches'});
			$cmd .= " -w ".$data->{'bmatchesOptions'}->{'strand'}           if defined($data->{'bmatchesOptions'}->{'strand'});
			$cmd .= " -n ".$data->{'bmatchesOptions'}->{'threads'}          if defined($data->{'bmatchesOptions'}->{'threads'});
			$cmd .= " -Q ".$data->{'bmatchesOptions'}->{'queueLength'}      if defined($data->{'bmatchesOptions'}->{'queueLength'});
			$cmd .= " -T ".$data->{'globalOptions'}->{'tmpDirectory'};
			$cmd .= " -o ".$output_id;
			$cmd .= " -d ".$data->{'globalOptions'}->{'outputDirectory'};
			$cmd .= " -t"                                                   if defined($data->{'globalOptions'}->{'timing'});

			# Submit the job
			my @a = (); # empty array for job dependencies
			push(@$qsub_ids, SubmitJob($run_file, $quiet, $cmd, $data, 'bmatchesOptions', $output_id, \@a));
			push(@$output_ids, $output_id);
			$cur_read_num_start += $data->{'globalOptions'}->{'numReadsPerFASTQ'}->{'bmatchesSplit'};
			$cur_read_num_end += $data->{'globalOptions'}->{'numReadsPerFASTQ'}->{'bmatchesSplit'};
		}
	}
}

# One dependent id for each output id
sub CreateJobsBalign {
	my ($data, $quiet, $dependent_ids, $qsub_ids, $input_ids, $output_ids) = @_;

	# The number of bmatches to perform per read file
	my $num_split_files = int(0.5 + ($data->{'globalOptions'}->{'numReadsPerFASTQ'}->{'bmatchesSplit'}/$data->{'globalOptions'}->{'numReadsPerFASTQ'}->{'balignSplit'}));

	# Go through each
	for(my $i=0;$i<scalar(@$input_ids);$i++) {
		my ($cur_read_num_start, $cur_read_num_end) = (1, $data->{'globalOptions'}->{'numReadsPerFASTQ'}->{'balignSplit'});
		my ($output_id_read_num_start, $output_id_read_num_end) = (0, 0);
		my $dependent_job = $dependent_ids->[$i];
		my $input_id = $input_ids->[$i];
		my $input_id_no_read_num ="";
		if($input_id =~ m/(.+)\.(\d+)\-\d+$/) {
			$input_id_no_read_num = $1;
			$output_id_read_num_start += $2; 
			$output_id_read_num_end += $2 + $data->{'globalOptions'}->{'numReadsPerFASTQ'}->{'balignSplit'} - 1;
		}
		else {
			die;
		}
		for(my $j=0;$j<$num_split_files;$j++) {
			my $output_id = "$input_id_no_read_num.$output_id_read_num_start-$output_id_read_num_end";
			my $bmf_file = GetMatchesFile($data, $input_id);
			my $run_file = CreateRunFile($data, 'balign', $output_id);

			my $cmd = "";
			$cmd .= $data->{'globalOptions'}->{'bfastBin'}."balign/"        if defined($data->{'globalOptions'}->{'bfastBin'});
			$cmd .= "balign";
			$cmd .= " -r ".$data->{'globalOptions'}->{'brgNT'};
			$cmd .= " -m $bmf_file";
			$cmd .= " -x ".$data->{'balignOptions'}->{'scoringMatrix'}      if defined($data->{'balignOptions'}->{'scoringMatrix'});
			$cmd .= " -a ".$data->{'balignOptions'}->{'localAlignmentType'} if defined($data->{'balignOptions'}->{'localAlignmentType'});
			$cmd .= " -A 1"                                                 if ("CS" eq $data->{'globalOptions'}->{'space'});
			$cmd .= " -O ".$data->{'balignOptions'}->{'offset'}             if defined($data->{'balignOptions'}->{'offset'});
			$cmd .= " -M ".$data->{'balignOptions'}->{'maxNumMatches'}      if defined($data->{'balignOptions'}->{'maxNumMatches'});
			$cmd .= " -q ".$data->{'balignOptions'}->{'mismatchQuality'}    if defined($data->{'balignOptions'}->{'mismatchQuality'});
			$cmd .= " -n ".$data->{'balignOptions'}->{'threads'}            if defined($data->{'balignOptions'}->{'threads'});
			$cmd .= " -Q ".$data->{'balignOptions'}->{'queueLength'}        if defined($data->{'balignOptions'}->{'queueLength'});
			$cmd .= " -l ".$data->{'balignOptions'}->{'pairedEndLength'}    if defined($data->{'balignOptions'}->{'pairedEndLength'});
			$cmd .= " -L ".$data->{'balignOptions'}->{'mirroringType'}      if defined($data->{'balignOptions'}->{'mirroringType'});
			$cmd .= " -F"                                                   if defined($data->{'balignOptions'}->{'forceMirror'});
			$cmd .= " -s $cur_read_num_start -e $cur_read_num_end";
			$cmd .= " -T ".$data->{'globalOptions'}->{'tmpDirectory'};
			$cmd .= " -o ".$output_id;
			$cmd .= " -d ".$data->{'globalOptions'}->{'outputDirectory'};
			$cmd .= " -t"                                                   if defined($data->{'globalOptions'}->{'timing'});

			# Submit the job
			my @a = (); push(@a, $dependent_job);
			push(@$qsub_ids, SubmitJob($run_file, $quiet, $cmd, $data, 'balignOptions', $output_id, \@a));
			push(@$output_ids, $output_id);
			$cur_read_num_start += $data->{'globalOptions'}->{'numReadsPerFASTQ'}->{'balignSplit'};
			$cur_read_num_end += $data->{'globalOptions'}->{'numReadsPerFASTQ'}->{'balignSplit'};
			$output_id_read_num_start += $data->{'globalOptions'}->{'numReadsPerFASTQ'}->{'balignSplit'};
			$output_id_read_num_end += $data->{'globalOptions'}->{'numReadsPerFASTQ'}->{'balignSplit'};
		}
	}
}

sub CreateJobsBpostprocess {
	my ($data, $quiet, $dependent_ids, $qsub_ids, $output_ids) = @_;

	# Go through each
	for(my $i=0;$i<scalar(@$output_ids);$i++) {
		my $output_id = $output_ids->[$i];
		my $dependent_job = $dependent_ids->[$i];
		my $baf_file = GetAlignFile($data, $output_id);
		my $run_file = CreateRunFile($data, 'bpostprocess', $output_id);

		my $cmd = "";
		$cmd .= $data->{'globalOptions'}->{'bfastBin'}."bpostprocess/" if defined($data->{'globalOptions'}->{'bfastBin'});
		$cmd .= "bpostprocess";
		$cmd .= " -r ".$data->{'globalOptions'}->{'brgNT'};
		$cmd .= " -i $baf_file";
		$cmd .= " -a ".$data->{'bpostprocessOptions'}->{'algorithm'}   if defined($data->{'bpostprocessOptions'}->{'algorithm'});
		$cmd .= " -Q ".$data->{'bpostprocessOptions'}->{'queueLength'} if defined($data->{'bpostprocessOptions'}->{'queueLength'});
		$cmd .= " -o ".$output_id;
		$cmd .= " -O 3"; # always SAM format
		$cmd .= " -d ".$data->{'globalOptions'}->{'outputDirectory'};
		$cmd .= " -t"                                                  if defined($data->{'globalOptions'}->{'timing'});

		# Submit the job
		my @a = (); push(@a, $dependent_job);
		push(@$qsub_ids, SubmitJob($run_file, $quiet, $cmd, $data, 'bpostprocessOptions', $output_id, \@a));
	}
}

sub CreateJobsSamtools {
	my ($data, $quiet, $dependent_ids, $output_ids) = @_;
	my @qsub_ids = ();
	my ($cmd, $run_file, $output_id);

	# Go through each
	for(my $i=0;$i<scalar(@$output_ids);$i++) {
		$output_id = $output_ids->[$i];
		my $dependent_job = $dependent_ids->[$i];
		my $sam_file = GetReportedFile($data, $output_id);
		$run_file = CreateRunFile($data, 'samtools', $output_id);

		$cmd = "";
		$cmd .= $data->{'globalOptions'}->{'samtoolsBin'}            if defined($data->{'globalOptions'}->{'samtoolsBin'});
		$cmd .= "samtools view -S -b";
		$cmd .= " -T ".$data->{'globalOptions'}->{'referenceFasta'};
		$cmd .= " $sam_file | ";
		$cmd .= $data->{'globalOptions'}->{'samtoolsBin'}            if defined($data->{'globalOptions'}->{'samtoolsBin'});
		$cmd .= "samtools sort";
		$cmd .= " -m ".$data->{'samtoolsOptions'}->{'maximumMemory'} if defined($data->{'samtoolsOptions'}->{'maximumMemory'});
		$cmd .= " - ".$data->{'globalOptions'}->{'outputDirectory'};
		$cmd .= "bfast.reported.$output_id";

		# Submit the job
		my @a = (); push(@a, $dependent_job);
		push(@qsub_ids, SubmitJob($run_file, $quiet, $cmd, $data, 'samtoolsOptions', $output_id, \@a));
	}

	# Merge script(s)
	# Note: there could be too many dependencies, so lets just create dummy jobs to "merge" the dependencies
	my $merge_lvl = 0;
	while(MERGE_LOG_BASE < scalar(@qsub_ids)) { # while we must merge
		$merge_lvl++;
		my $ctr = 0;
		my @cur_ids = @qsub_ids;
		@qsub_ids = ();
		for(my $i=0;$i<scalar(@cur_ids);$i+=MERGE_LOG_BASE) {
			$ctr++;
			# Get the subset of dependent jobs
			my @dependent_jobs = ();
			for(my $j=$i;$j<scalar(@cur_ids) && $j<$i+MERGE_LOG_BASE;$j++) {
				push(@dependent_jobs, $cur_ids[$j]);
			}
			# Create the command
			$output_id = "merge.".$data->{'globalOptions'}->{'outputID'}.".$merge_lvl.$ctr";
			$run_file = $data->{'globalOptions'}->{'runDirectory'}."samtools.".$output_id.".sh";
			$cmd = "echo \"Merging $merge_lvl / $ctr\"\n";
			push(@qsub_ids, SubmitJob($run_file, $quiet, $cmd, $data, 'samtoolsOptions', $output_id, \@dependent_jobs));
		}
	}

	$output_id = "merge.".$data->{'globalOptions'}->{'outputID'};
	$run_file = $data->{'globalOptions'}->{'runDirectory'}."samtools.".$output_id.".sh";
	$cmd = $data->{'globalOptions'}->{'samtoolsBin'} if defined($data->{'globalOptions'}->{'samtoolsBin'});
	$cmd .= "samtools merge";
	$cmd .= " ".$data->{'globalOptions'}->{'outputDirectory'}."bfast.".$data->{'globalOptions'}->{'outputID'}.".bam";
	$cmd .= " ".$data->{'globalOptions'}->{'outputDirectory'}."bfast.reported.*bam";
	SubmitJob($run_file , $quiet, $cmd, $data, 'samtoolsOptions', $output_id, \@qsub_ids);
}

sub SubmitJob {
	my ($run_file, $quiet, $command, $data, $type, $output_id, $dependent_job_ids) = @_;
	$output_id = "$type.$output_id"; $output_id =~ s/Options//g;

	if(!$quiet) {
		print STDERR "[bfast submit] RUNFILE=$run_file\n";
	}
	open(FH, ">$run_file") or die("Error.  Could not open $run_file for writing!\n");
	print FH "$command\n";
	close(FH);

	# Create qsub command
	my $qsub = "qsub";
	if(0 < scalar(@$dependent_job_ids)) {
		$qsub .= " -hold_jid ".join(",", @$dependent_job_ids)         if ("SGE" eq $data->{'globalOptions'}->{'queueType'});
		$qsub .= " -W depend=afterok:".join(":", @$dependent_job_ids) if ("PBS" eq $data->{'globalOptions'}->{'queueType'});
	}
	if(defined($data->{$type}->{'threads'}) && 1 < $data->{$type}->{'threads'}) {
		$qsub .= " -pe serial ".$data->{$type}->{'threads'}     if ("SGE" eq $data->{'globalOptions'}->{'queueType'});;
		$qsub .= " -l nodes=1:ppn=".$data->{$type}->{'threads'} if ("PBS" eq $data->{'globalOptions'}->{'queueType'});;
	}
	$qsub .= " -q ".$data->{$type}->{'qsubQueue'} if defined($data->{$type}->{'qsubQueue'});
	$qsub .= " -N $output_id -o $run_file.out -e $run_file.err $run_file";

	# Submit the qsub command
	my $qsub_id=`$qsub`;
	chomp($qsub_id);
	die("Error submitting QSUB_COMMAND=$qsub\n") unless (0 < length($qsub_id));

	if(!$quiet) {
		print STDERR "[bfast submit] NAME=$output_id QSUBID=$qsub_id\n";
	}

	return $qsub_id;
}

__END__
=head1 SYNOPSIS

bfast.submit.pl [options] 

=head1 OPTIONS

=over 8

=item B<-help>
Print a brief help message and exits.

=item B<-schema>
Print the configuration XML schema.

=item B<-man>
Prints the manual page and exits.

=item B<-quiet>
Do not print any submit messages.

=item B<-config>
The XML configuration file.

=back

=head1 DESCRIPTION

B<bfast.submit.pl> will create the necessary shell scripts for B<BFAST> to be
run on a supported cluster (SGE and PBS).  It will also submit each script
supporting job dependencies.  The input to B<bfast.submit.pl> is an XML
configuration file.  To view the schema for this file, please use the 
I<-schema> option.

To use this script to run B<BFAST>, the necessary input files must be created. 
This includes creating a BFAST reference genome file (with an additional color space version
for ABI SOLiD data), the BFAST index file(s), the main index(es) list, and a 
samtools indexed reference FASTA file (using 'samtools faidx').  Additionally,
the reads, if not already in B<FASTQ> format then the input files must be 
properly reformatted and can be optionally split for parallel processing 
(please observe B<BFAST>'s paired-end or mate-end B<FASTQ> 
format).  Optional input files must also be created before using this script.
For a description of how to create these files, please see the 
B<bfast-book.pdf> in the B<BFAST> distribution.

The behaviour of this script is as follows.  For each B<FASTQ> file found in the
reads directory, one index search process using B<bmatches>, one local alignment
process using B<balign> will be performed, one post-processing process using 
B<bpostprocess>, and one import to the B<SAM> format using B<SAMtools> (see 
http://samtools.sourceforge.net) will be performed.  Finally, we merge all B<SAM> 
files that have been created for each input B<FASTQ> file.  Observe that all the
B<SAM> files will be sorted by location.  We submit each job separately using
the job dependency capabilities of the cluster scheduler.  All output files
will be created in the output directory, all run files as well as the stdout and
stderr streams will be found in the run files directory, and all temporary files
will be created in the temporary directory.

Please report all bugs to nhomer@cs.ucla.edu or bfast-help@lists.sourceforge.net.

=head1 REQUIREMENTS

This script requires the XML::Simple library, which can be found at:
http://search.cpan.org/dist/XML-Simple

=cut
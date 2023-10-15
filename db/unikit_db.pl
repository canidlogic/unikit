#!/usr/bin/env perl
use v5.16;
use warnings;

=head1 NAME

unikit_db.pl - Generate Unikit data tables from the Unicode Character
Database.

=head1 SYNOPSIS

  unikit_db.pl case UCD/CaseFolding.txt > case_table.txt

=head1 DESCRIPTION

The first parameter to this script selects the kind of Unikit data table
that should be generated.  Following this initial parameter come one or
more paths to specific data files in the Unicode Character Database
which will be parsed.  The generated result is written to standard
output.

The following subsections document the kind of data tables that can be
generated and their format.

=head2 Case folding table

The case folding table is generated with the C<case> invocation of the
script.

The case folding table is split into two separate tables, both of which
have the exact same format.  One table is used for codepoints in range
U+0000 to U+FFFF, while the other table is used for codepoints in range
U+10000 to U+1FFFF.

(There are no case folding records beyond U+1FFFF.  Furthermore, all
case folding records in U+0000 to U+FFFF only map to other codepoints in
that same range, and all case folding records in U+10000 to U+1FFFF only
map to other codepoints in that same range.  The two ranges can
therefore be handled separately.)

Each table is expressed as an index of exactly 256 unsigned 8-bit
integers, followed by a variable-length array of unsigned 16-bit
integers.

The most significant 8 bits of the 16 least significant bits of the
codepoint are used as an offset into the index table.  If the index
table record is 255, then the table contains no case foldings for
codepoints in that range.  Otherwise, the index of the table record
selects a subtable in range 0 to 254.

Subtables are stored within the 16-bit array.  Each subtable has exactly
256 records, and each record contains exactly three 16-bit integers.
Therefore, each subtable consists of 768 16-bit integers.  Subtable
index N starts at the integer offset 768 times N within the 16-bit
array.

The least significant 8 bits of the codepoints are used as a record
index within the subtable.  If the first integer of the record is zero,
then there is no case folding for that codepoint.  If the first integer
is non-zero and the second is zero, then the case-folding is to a single
codepoint given in the first integer.  If the first two integers are
non-zero and the third is zero, then the case-folding is to a sequence
of two codepoints given in the first two integers.  If all three
integers are non-zero, then the case-folding is to a sequence of three
codepoints.

=cut

# ===========
# Subprograms
# ===========

# do_case(path_casefold)
# ----------------------
#
# Generate the case-folding table.
#
sub do_case {
  # Get parameters
  ($#_ == 0) or die "Bad call";
  
  my $path_casefold = shift;
  (not ref($path_casefold)) or die "Bad call";
  (-f $path_casefold) or die "Failed to find file '$path_casefold'";
  
  # Open the data file in raw byte mode
  open(my $fh, "< :raw", $path_casefold) or
    die "Failed to open file '$path_casefold'";
  
  # Case folding tables have 256 records initially all set to undef;
  # when subtables are defined as array references, each has 256 records
  # initially all set to undef; when records defined in those subtables,
  # they are subarrays with one to three codepoint records
  my @fold_upper;
  my @fold_lower;
  
  for(my $i = 0; $i < 256; $i++) {
    push @fold_upper, (undef);
    push @fold_lower, (undef);
  }
  
  # Parse records
  for(
      my $ltext = readline($fh);
      defined $ltext;
      $ltext = readline($fh)) {
    
    # Drop line break, comments, and trim whitespace
    chomp $ltext;
    $ltext =~ s/#.*//;
    $ltext =~ s/^\s+//;
    $ltext =~ s/\s+$//;
    
    # Skip if blank
    (length($ltext) > 0) or next;
    
    # Split record into fields
    ($ltext =~ /^([^;]*);([^;]*);([^;]*);$/) or die "Invalid record";
    my $ucode = $1;
    my $ftype = $2;
    my $mapping = $3;
    
    # Parse codepoint field
    ($ucode =~ /^\s*([0-9A-Fa-f]{1,6})\s*$/) or
      die "Invalid codepoint field";
    
    $ucode = hex($1);
    (($ucode >= 0) and ($ucode <= 0x1FFFF)) or
      die "Codepoint out of range";
    
    # Parse folding type field
    ($ftype =~ /^\s*([CFST])\s*$/) or
      die "Mapping type invalid";
    $ftype = $1;
    
    # Parse mapping into sequence of one to three codepoints
    $mapping =~ s/^\s+//;
    $mapping =~ s/\s+$//;
    
    my @cpa = split /\s+/, $mapping;
    ((scalar(@cpa) >= 1) and (scalar(@cpa) <= 3)) or
      die "Mapping length out of range";
    
    for my $cv (@cpa) {
      ($cv =~ /^[0-9A-Fa-f]{1,6}$/) or
        die "Invalid codepoint in mapping";
      $cv = hex($cv);
      (($cv >= 0) and ($cv <= 0x1FFFF)) or
        die "Mapping codepoint out of range";
    }
    
    # Ignore all records except for C and F, since we are only
    # interested in full case folding
    (($ftype eq 'C') or ($ftype eq 'F')) or next;
    
    # Determine whether we are in the low range table (0) or the high
    # range table (1) and adjust both the codepoint and all codepoints
    # in the mapping so that they are all in range 0x0000 to 0xFFFF
    # within the chosen range
    my $tchoice = 0;
    if ($ucode > 0xFFFF) {
      $tchoice = 1;
    }
    
    if ($tchoice == 1) {
      # Upper table, so everything should be an offset from 0x10000
      $ucode = $ucode - 0x10000;
      (($ucode >= 0) and ($ucode <= 0xffff)) or die;
      
      for my $cv (@cpa) {
        $cv -= 0x10000;
        (($cv >= 0) and ($cv <= 0xffff)) or
          die "Mapping codepoint not in correct subrange";
      }
      
    } elsif ($tchoice == 0) {
      # Lower table, so everything should already be in lower range
      (($ucode >= 0) and ($ucode <= 0xffff)) or die;
      
      for my $cv (@cpa) {
        (($cv >= 0) and ($cv <= 0xffff)) or
          die "Mapping codepoint not in correct subrange";
      }
      
    } else {
      die;
    }
    
    # Get the correct table reference
    my $fold;
    if ($tchoice == 1) {
      $fold = \@fold_upper;
    
    } elsif ($tchoice == 0) {
      $fold = \@fold_lower;
      
    } else {
      die;
    }
    
    # Parse ucode into an index and an address
    my $idx  = ($ucode >> 8) & 0xff;
    my $addr =  $ucode       & 0xff;
    
    # Define subtable if not yet defined
    unless (defined $fold->[$idx]) {
      $fold->[$idx] = [];
      for(my $i = 0; $i < 256; $i++) {
        push @{$fold->[$idx]}, (undef);
      }
    }
    
    # Check record not yet defined
    (not defined $fold->[$idx]->[$addr]) or
      die "Duplicate codepoint record";
    
    # Add the new record
    $fold->[$idx]->[$addr] = \@cpa;
  }
  
  # Define the index tables and data tables
  my @index_upper;
  my @index_lower;
  
  my @data_upper;
  my @data_lower;
  
  for(my $i = 0; $i < 2; $i++) {
    # Get the current index, data, and folding table
    my $fold;
    my $index;
    my $data;
    
    if ($i == 0) {
      $fold = \@fold_upper;
      $index = \@index_upper;
      $data = \@data_upper;
      
    } elsif ($i == 1) {
      $fold = \@fold_lower;
      $index = \@index_lower;
      $data = \@data_lower;
      
    } else {
      die;
    }
    
    # The subtable count starts at zero
    my $count = 0;
    
    # Define each index record
    for(my $j = 0; $j < 256; $j++) {
      # If this subtable not defined, add a 255 entry and skip it
      unless (defined $fold->[$j]) {
        push @$index, (255);
        next;
      }
      
      # We need another subtable, so check we haven't reached the limit
      ($count < 255) or die "Too many subtables";
      
      # Use the current count as the index entry and then increment
      # count
      push @$index, ($count);
      $count++;
    }
    
    # Initialize the data table with all records at zero
    my $rec_count = $count * 256;
    for(my $k = 0; $k < $rec_count; $k++) {
      push @$data, (0, 0, 0);
    }
    
    # Copy all records from the folding table into the data table
    for(my $m = 0; $m < 256; $m++) {
      # Skip this index record if undefined
      (defined $fold->[$m]) or next;
      
      # Get the subtable index
      my $subtable = $index->[$m];
      ($subtable < 255) or die;
      $subtable *= 768;
      
      # Define all subtable records
      for(my $n = 0; $n < 256; $n++) {
        # Skip this record if undefined
        (defined $fold->[$m]->[$n]) or next;
        
        # Get record and compute offset in data table
        my $rec = $fold->[$m]->[$n];
        my $offs = ($n * 3) + $subtable;
        
        # Copy subtable record
        for my $r (@$rec) {
          $data->[$offs] = $r;
          $offs++;
        }
      }
    }
  }
  
  # Print macro declarations
  print "#define B(X) UINT8_C(X)\n";
  print "#define W(X) UINT16_C(X)\n";
  print "\n";
  
  # Print index tables
  for(my $i = 0; $i < 2; $i++) {
    # Print header and get index table
    my $idx;
    if ($i == 0) {
      $idx = \@index_lower;
      print "static uint8_t FOLD_INDEX_LOWER[256] = {";
      
    } elsif ($i == 1) {
      $idx = \@index_upper;
      print "static uint8_t FOLD_INDEX_UPPER[256] = {";
      
    } else {
      die;
    }
    
    # Print records in rows of 8 entries
    for(my $j = 0; $j < 256; $j++) {
      # If this is not the first record, add a comma as a separator from
      # the previous
      unless ($j < 1) {
        print ",";
      }
      
      # If this record is zero or a multiple of 8, line break and  
      # indent, else space
      if (($j % 8) == 0) {
        print "\n  ";
      } else {
        print " ";
      }
      
      # Print this record
      printf "B(%3d)", $idx->[$j];
    }
    
    # Print footer
    print "\n};\n\n";
  }
  
  # Print data tables
  for(my $i = 0; $i < 2; $i++) {
    # Print header and get data table
    my $data;
    if ($i == 0) {
      $data = \@data_lower;
      printf "static uint16_t FOLD_DATA_LOWER[%d] = {", scalar(@$data);
      
    } elsif ($i == 1) {
      $data = \@data_upper;
      printf "static uint16_t FOLD_DATA_UPPER[%d] = {", scalar(@$data);
      
    } else {
      die;
    }
    
    # Print records in rows of 6 entries
    for(my $j = 0; $j < scalar(@$data); $j++) {
      # If this is not the first record, add a comma as a separator from
      # the previous
      unless ($j < 1) {
        print ",";
      }
      
      # If this record is zero or a multiple of 6, line break and  
      # indent, else space
      if (($j % 6) == 0) {
        print "\n  ";
      } else {
        print " ";
      }
      
      # Print this record
      printf "W(0x%04x)", $data->[$j];
    }
    
    # Print footer
    print "\n};\n\n";
  }
  
  # Close the data file
  close($fh) or warn "Failed to close file";
}

# ==================
# Program entrypoint
# ==================

# Get mode
#
(scalar(@ARGV) > 0) or die "Expecting program arguments";
my $script_mode = shift @ARGV;

# Handle the different modes
#
if ($script_mode eq 'case') {
  # Case folding mode
  (scalar(@ARGV) == 1) or die "Wrong number of arguments for mode";
  my $path = shift @ARGV;
  
  do_case($path);
  
} else {
  die "Unknown script mode '$script_mode'";
}

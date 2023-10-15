#!/usr/bin/env perl
use v5.16;
use warnings;

use Scalar::Util qw(looks_like_number);

use GeneralTable;
use Trie;

=head1 NAME

unikit_db.pl - Generate Unikit data tables from the Unicode Character
Database.

=head1 SYNOPSIS

  unikit_db.pl case pretty UCD/CaseFolding.txt > case_table.txt
  unikit_db.pl genchar b64 UCD/UnicodeData.txt > genchar.txt
  unikit_db.pl astral pretty UCD/UnicodeData.txt > astral.txt

=head1 DESCRIPTION

The first parameter to this script selects the kind of Unikit data table
that should be generated.  The second parameter to this script is either
C<pretty> or C<base64> to select whether output should be in a readable
format or in a base-64 C string literal format.

Following these initial parameters come one or more paths to specific
data files in the Unicode Character Database which will be parsed.  The
generated result is written to standard output.  This script is written
against version 15.1 of the Unicode Character Database.

The following subsections document the kind of data tables that can be
generated and their format.

=head2 Case folding table

The case folding table is generated with the C<case> invocation of the
script.

The case folding table consists of two compiled tries of depth 4 and a
data array.  See the Trie module for the details of how a compiled Trie
works.

The first trie covers codepoints U+0000 to U+FFFF, while the second trie
covers codepoints U+10000 to U+1FFFF.  There are no case folding records
for codepoints greater than U+1FFFF.  The key to use in each trie is the
four nybbles of the lower 16 bits of the codepoint.

The mapped value in both tries is a data key into the data array.  The
two least significant bits are one less than the number of codepoints in
the case folding expansion.  When the data key is shifted right by two
bits, it yields the integer offset within the data array that the
expansion codepoint sequence begins at.

All codepoints in the data array are stored with only their 16 least
significant bits.  The 17th bit should be set to the same value as the
17th bit of the input codepoint.  That is, codepoints in range U+0000 to
U+FFFF map exclusively to other codepoints in that range, and codepoints
in range U+10000 to U+1FFFF map exclusively to other codepoints in that
range.

=head2 General character table

The general character table is generated with the C<genchar> invocation
of the script.

The general character table only covers codepoints in range U+0100 to
U+1FFFF.  Codepoints in range U+0000 to U+00FF are high frequency and
handled with a special lookup table.  Codepoints U+20000 and above
consist of broad ranges of similar codepoints that are not handled well
with a trie.

The general character table covers all general categories except for the
following:

  Lo Ll So Cs Co Cn

The C<Lo>, C<Ll>, and C<So> categories are large categories that are
better handled with a bitmap that assigns two bits to each codepoints to
select between those three categories and a fourth value indicating not
covered by the bitmap.

The C<Cs> and C<Co> categories are represented by broad ranges that are
easy to do range tests for.

The C<Cn> category is assigned to anything that is in Unicode range but
not in any of the tables.

The general character table is represented by two tries of depth 4.  The
first trie has keys representing codepoints in range U+0000 to U+FFFF,
while the second trie has keys representing codepoints in range U+10000
to U+1FFFF.

The 16-bit values that these tries map to are the two ASCII letters of
the general category, with the first letter in the most significant byte
and the second letter in the least significant byte.

=head2 Astral character table

The astral character table is generated with the C<astral> invocation of
the script.

The astral character table only covers codepoints in range U+20000 to
U+EFFFF.  The only codepoints greater than U+EFFFF are covered by two
ranges:  U+F0000 to U+FFFFD and U+100000 to U+10FFFD are both private
use ranges with general category C<Co>.

These astral codepoints are not frequently used and mostly fall into
broad ranges.  Sorted range tables are therefore used.  There are three
sorted range tables.  The first covers U+20000 to U+2FFFF, the second
covers U+30000 to U+3FFFF, and the third covers U+E0000 to U+EFFFF.  All
other codepoints in the astral range are undefined.

Range tables are expressed as arrays of unsigned 16-bit integers.  Each
range record is exactly three integer elements.  The first element is
the lower bound of a range, the second element is the upper bound of a
range, and the third element is the two ASCII letters of the general
category, with the first letter in the most signficant byte and the
second letter in the least significant byte.

Only the 16 least significant bits of the codepoint are stored in the
records.  The upper bound is included in the range, and must be greater
than or equal to the lower bound.  For all records after the first in a
range table, the lower bound must be greater than the upper bound of the
previous record.

=cut

# ===============
# Local functions
# ===============

# isInteger(val)
# --------------
#
# Check that the given value is an integer.  Return 1 if an integer or 0
# if not.
#
sub isInteger {
  ($#_ == 0) or die "Bad call";
  my $val = shift;
  
  looks_like_number($val) or return 0;
  (int($val) == $val) or return 0;
  
  return 1;
}

# base64_table()
# --------------
#
# Return an array of 64 strings in list context.  Each string is exactly
# one character long, corresponding to one of the base-64 digits.
#
sub base64_table {
  ($#_ < 0) or die "Bad call";
  
  my @result;
  
  for(my $i = 0; $i < 26; $i++) {
    push @result, (chr(ord('A') + $i));
  }
  
  for(my $i = 0; $i < 26; $i++) {
    push @result, (chr(ord('a') + $i));
  }
  
  for(my $i = 0; $i < 10; $i++) {
    push @result, (chr(ord('0') + $i));
  }
  
  push @result, ('+');
  push @result, ('/');
  
  return @result;
}

# encode_category(cc)
# -------------------
#
# Given a category code consisting of an uppercase ASCII letter followed
# by a lowercase ASCII letter, return an integer in range 0x0000 to
# 0xFFFF that has the uppercase ASCII letter in the 8 most significant
# bits and the lowercase ASCII letter in the 8 least significant bits.
#
sub encode_category {
  ($#_ == 0) or die "Bad call";
  
  my $cc = shift;
  (not ref($cc)) or die "Bad call";
  
  ($cc =~ /^([A-Z])([a-z])$/) or die "Category out of range";
  my $upper = ord($1);
  my $lower = ord($2);
  
  return (($upper << 8) | $lower);
}

# print_array16(\@ar, $use_b64)
# -----------------------------
#
# Print an array of unsigned 16-bit integers.
#
# ar is the array reference.  It must have at least one element and at
# most 65536 elements.  Each element must be a scalar integer that is in
# range 0x0000 to 0xFFFF.
#
# use_b64 is an integer that is either 0 (false) or 1 (true).  If false,
# then the array is printed in pretty format.  If true, then the array
# is printed in base64 C literal format.
#
# Pretty format begins each line with four base-16 characters indicating
# the integer offset of the first integer in the line.  There are up to
# eight integers on each line.
#
# Base64 format has a double-quoted C string literal on each line.  Each
# line's literal contains up to 64 base-64 characters.  16-bit integers
# are encoded in big endian format.
#
sub print_array16 {
  # Get parameters
  ($#_ == 1) or die "Bad call";
  
  my $ar = shift;
  (ref($ar) eq 'ARRAY') or die "Bad parameter type";
  ((scalar(@$ar) > 0) and (scalar(@$ar) <= 65536)) or
    die "Array length out of range";
  
  my $use_b64 = shift;
  isInteger($use_b64) or die "Bad parameter type";
  (($use_b64 == 0) or ($use_b64 == 1)) or die "Parameter out of range";
  
  # Print requested format
  if ($use_b64) {
    # Base-64 C literal format, so each line will encode up to 24 16-bit
    # integers, which will result in up to 64 base-64 digits per line
    my @encode_table = base64_table();
    for(my $base = 0; $base < scalar(@$ar); $base += 24) {
      # Print line header
      print "  \"";
      
      # Decode the integers on this line into an array of bytes
      my @bytes;
      for(
          my $i = $base;
          ($i < $base + 24) and ($i < scalar(@$ar));
          $i++) {
        my $ival = $ar->[$i];
        push @bytes, (
          ($ival >> 8) & 0xff,
           $ival       & 0xff
        );
      }
      
      # Determine how many full triplets of bytes there are, and how
      # many bytes remain at the end that are not in triplets
      my $triplets = int(scalar(@bytes) / 3);
      my $rem = scalar(@bytes) % 3;
      
      # Encode each full triplet into four base-64 digits
      for(my $i = 0; $i < $triplets * 3; $i += 3) {
        my @comp = (
          ($bytes[$i] >> 2),
          (($bytes[$i] & 0x3) << 4) | ($bytes[$i + 1] >> 4),
          (($bytes[$i + 1] & 0x0f) << 2) | ($bytes[$i + 2] >> 6),
          ($bytes[$i + 2] & 0x3f)
        );
        
        printf "%s%s%s%s",
          $encode_table[$comp[0]],
          $encode_table[$comp[1]],
          $encode_table[$comp[2]],
          $encode_table[$comp[3]];
      }
      
      # Handle remaining bytes cases
      if ($rem == 2) {
        # Two bytes remain, so we will have three base-16 digits
        my @comp = (
          ($bytes[-2] >> 2),
          (($bytes[-2] & 0x3) << 4) | ($bytes[-1] >> 4),
          (($bytes[-1] & 0x0f) << 2)
        );
        
        # Print the three base-16 digits with an equals sign for padding
        printf "%s%s%s=",
          $encode_table[$comp[0]],
          $encode_table[$comp[1]],
          $encode_table[$comp[2]];
        
      } elsif ($rem == 1) {
        # One byte remains, so we will have two base-16 digits
        my @comp = (
          ($bytes[-1] >> 2),
          (($bytes[-1] & 0x3) << 4)
        );
        
        # Print the two base-16 digits with two equals signs for padding
        printf "%s%s==",
          $encode_table[$comp[0]],
          $encode_table[$comp[1]];
        
      } else {
        # The only other possibility is zero remainder, in which case we
        # need not do anything
        ($rem == 0) or die;
      }
      
      # Print line trailer
      print "\"\n";
    }
    
  } else {
    # Pretty format, so print each integer individually
    for(my $i = 0; $i < scalar(@$ar); $i++) {
      # If not first element, print comma to close previous element
      if ($i > 0) {
        print ",";
      }
      
      # If element index is multiple of eight but not zero, then print
      # line break
      if ((($i % 8) == 0) and ($i > 0)) {
        print "\n";
      }
      
      # If element index is multiple of eight, including zero, then
      # print line header; else, print space separator
      if (($i % 8) == 0) {
        printf "%04x: ", $i;
      } else {
        print " ";
      }
      
      # Get current element and check
      my $e = $ar->[$i];
      isInteger($e) or die "Wrong element type";
      (($e >= 0) and ($e <= 0xFFFF)) or die "Element out of range";
      
      # Print current element
      printf "0x%04x", $e;
    }
    
    # Print final line break
    print "\n";
  }
}

# ===========
# Subprograms
# ===========

# do_astral(path_unicodedata, use_b64)
# ------------------------------------
#
# Generate the astral character table.
#
sub do_astral {
  # Get parameters
  ($#_ == 1) or die "Bad call";
  
  my $path_ucdata = shift;
  (not ref($path_ucdata)) or die "Bad call";
  (-f $path_ucdata) or die "Failed to find file '$path_ucdata'";
  
  my $use_b64 = shift;
  isInteger($use_b64) or die "Bad call";
  (($use_b64 == 0) or ($use_b64 == 1)) or die "Bad param";
  
  # Open general table parser
  my $parse = GeneralTable->parse($path_ucdata);
  
  # Define range tables
  my @table_2;
  my @table_3;
  my @table_e;
  
  # Parse records
  for(my $rec = $parse->readRec; defined $rec; $rec = $parse->readRec) {
    
    # Ignore records with unassigned category
    ($rec->{'gencat'} ne 'Cn') or next;
    
    # Categorize record by range, skip records outside of range, and
    # select the proper range table and adjust boundaries to planar
    # offsets
    my $table;
    
    if (($rec->{'lbound'} >= 0x20000) and
        ($rec->{'ubound'} <= 0x2ffff)) {
      $table = \@table_2;
      
    } elsif (($rec->{'lbound'} >= 0x30000) and
              ($rec->{'ubound'} <= 0x3ffff)) {
      $table = \@table_3;
      
    } elsif (($rec->{'lbound'} >= 0xe0000) and
              ($rec->{'ubound'} <= 0xeffff)) {
      $table = \@table_e;
      
    } elsif (($rec->{'lbound'} > 0xeffff) or
              ($rec->{'ubound'} < 0x20000)) {
      # Record outside of astral range, so ignore
      next;
      
    } else {
      die "Record has invalid range";
    }
    
    $rec->{'lbound'} &= 0xffff;
    $rec->{'ubound'} &= 0xffff;
    
    # Get category code
    my $catcode = encode_category($rec->{'gencat'});
    
    # If selected table is empty, add record range as-is
    if (scalar(@$table) < 1) {
      push @$table, ([
            $rec->{'lbound'},
            $rec->{'ubound'},
            $catcode
      ]);
      next;
    }
    
    # If we got here, table is not empty, so check that the lower bound
    # of this new record is greater than the upper bound of the last
    # record
    ($table->[-1]->[1] < $rec->{'lbound'}) or
      die "Records out of order";
    
    # If we can merge this new record into the last range, do that
    if (($table->[-1]->[1] == $rec->{'lbound'} - 1) and
        ($table->[-1]->[2] == $catcode)) {
      $table->[-1]->[1] = $rec->{'ubound'};
      next;
    }
    
    # If we got here, add record range as-is
    push @$table, ([
      $rec->{'lbound'},
      $rec->{'ubound'},
      $catcode
    ]);
  }
  
  # Print each table
  for(my $i = 0; $i < 3; $i++) {
    # Print proper header and get correct table
    my $table;
    if ($i == 0) {
      print "Plane 2 table:\n\n";
      $table = \@table_2;
      
    } elsif ($i == 1) {
      print "\nPlane 3 table:\n\n";
      $table = \@table_3;
      
    } elsif ($i == 2) {
      print "\nPlane E table:\n\n";
      $table = \@table_e;
      
    } else {
      die;
    }
    
    # Flatten the table
    my @flat;
    for my $r (@$table) {
      push @flat, ($r->[0], $r->[1], $r->[2]);
    }
    
    # Print the table
    print_array16(\@flat, $use_b64);
  }
  
}

# do_genchar(path_unicodedata, use_b64)
# -------------------------------------
#
# Generate the general character table.  See the module documentation
# for exclusions that are not covered in the general character table.
#
sub do_genchar {
  # Get parameters
  ($#_ == 1) or die "Bad call";
  
  my $path_ucdata = shift;
  (not ref($path_ucdata)) or die "Bad call";
  (-f $path_ucdata) or die "Failed to find file '$path_ucdata'";
  
  my $use_b64 = shift;
  isInteger($use_b64) or die "Bad call";
  (($use_b64 == 0) or ($use_b64 == 1)) or die "Bad param";
  
  # Open general table parser
  my $parse = GeneralTable->parse($path_ucdata);
  
  # Define tries for upper and lower ranges, each with a nybble depth of
  # four
  my $table_upper = Trie->create(4);
  my $table_lower = Trie->create(4);
  
  # Parse records
  for(my $rec = $parse->readRec; defined $rec; $rec = $parse->readRec) {
    
    # Skip codepoints that are not in range U+0100 to U+20000
    ($rec->{'ubound'} >= 0x100) or next;
    ($rec->{'lbound'} < 0x20000) or next;
    
    # Skip categories that are excluded from this table
    my $ugc = $rec->{'gencat'};
    (($ugc ne 'Lo') and ($ugc ne 'Ll') and
      ($ugc ne 'So') and ($ugc ne 'Cs') and
      ($ugc ne 'Co') and ($ugc ne 'Cn')) or next;
    
    # If we got here, the record shouldn't be a range, and get the
    # unique codepoint
    ($rec->{'lbound'} == $rec->{'ubound'}) or die "Unexpected range";
    my $ucode = $rec->{'lbound'};
    
    # Convert category into 16-bit integer value
    my $ckey = encode_category($rec->{'gencat'});
  
    # Choose the proper subtable and adjust codepoint
    my $table;
    if (($ucode >= 0x0000) and ($ucode <= 0xffff)) {
      $table = $table_lower;
      
    } elsif (($ucode >= 0x10000) and ($ucode < 0x1FFFF)) {
      $table = $table_upper;
      $ucode -= 0x10000;
      
    } else {
      die;
    }
    
    # Split adjusted codepoint into nybble key
    my @mk = (
      $ucode >> 12,
      ($ucode >> 8) & 0x0f,
      ($ucode >> 4) & 0x0f,
      $ucode & 0x0f
    );
    
    # Add to table
    $table->add(\@mk, $ckey);
  }
  
  # Compile the two tries
  my @index_upper = $table_upper->compile;
  my @index_lower = $table_lower->compile;
  
  # Print the tries and the data table
  print "Lower index:\n\n";
  print_array16(\@index_lower, $use_b64);
  
  print "\nUpper index:\n\n";
  print_array16(\@index_upper, $use_b64);
}

# do_case(path_casefold, use_b64)
# -------------------------------
#
# Generate the case-folding table.
#
sub do_case {
  # Get parameters
  ($#_ == 1) or die "Bad call";
  
  my $path_casefold = shift;
  (not ref($path_casefold)) or die "Bad call";
  (-f $path_casefold) or die "Failed to find file '$path_casefold'";
  
  my $use_b64 = shift;
  isInteger($use_b64) or die "Bad call";
  (($use_b64 == 0) or ($use_b64 == 1)) or die "Bad param";
  
  # Open the data file in raw byte mode
  open(my $fh, "< :raw", $path_casefold) or
    die "Failed to open file '$path_casefold'";
  
  # Define tries for upper and lower ranges, each with a nybble depth of
  # four
  my $fold_upper = Trie->create(4);
  my $fold_lower = Trie->create(4);
  
  # Define data table which holds codepoint sequences
  my @data;
  
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
    
    # Make sure codepoint array has length in range 1 to 4
    ((scalar(@cpa) >= 1) and (scalar(@cpa) <= 4)) or
      die "Codepoint array length out of range";
    
    # Get the data offset of this sequence and make sure in range 0 to
    # 0x3FFE
    my $data_offs = scalar(@data);
    (($data_offs >= 0) and ($data_offs <= 0x3FFE)) or
      die "Too many codepoint sequences";
    
    # Shift the data offset over by two and use the two lower bits to
    # store one less than the codepoint array length to get the data key
    my $data_key = ($data_offs << 2) | (scalar(@cpa) - 1);

    # Append sequence to data
    push @data, (@cpa);

    # Get the correct trie reference
    my $fold;
    if ($tchoice == 1) {
      $fold = $fold_upper;
    
    } elsif ($tchoice == 0) {
      $fold = $fold_lower;
      
    } else {
      die;
    }
    
    # Parse ucode into a key of four nybbles
    my @ukey = (
      ($ucode >> 12) & 0x0f,
      ($ucode >>  8) & 0x0f,
      ($ucode >>  4) & 0x0f,
       $ucode        & 0x0f
    );
    
    # Add the new record
    $fold->add(\@ukey, $data_key);
  }
  
  # Compile the two tries
  my @index_upper = $fold_upper->compile;
  my @index_lower = $fold_lower->compile;
  
  # Print the tries and the data table
  print "Lower index:\n\n";
  print_array16(\@index_lower, $use_b64);
  
  print "\nUpper index:\n\n";
  print_array16(\@index_upper, $use_b64);
  
  print "\nData table:\n\n";
  print_array16(\@data, $use_b64);
  
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
  (scalar(@ARGV) == 2) or die "Wrong number of arguments for mode";
  my $style = shift @ARGV;
  my $path = shift @ARGV;
  
  if ($style eq 'pretty') {
    $style = 0;
  } elsif ($style eq 'base64') {
    $style = 1;
  } else {
    die "Unrecognized style '$style'";
  }
  
  do_case($path, $style);
  
} elsif ($script_mode eq 'genchar') {
  # General category table
  (scalar(@ARGV) == 2) or die "Wrong number of arguments for mode";
  my $style = shift @ARGV;
  my $path = shift @ARGV;
  
  if ($style eq 'pretty') {
    $style = 0;
  } elsif ($style eq 'base64') {
    $style = 1;
  } else {
    die "Unrecognized style '$style'";
  }
  
  do_genchar($path, $style);

} elsif ($script_mode eq 'astral') {
  # Astral character table
  (scalar(@ARGV) == 2) or die "Wrong number of arguments for mode";
  my $style = shift @ARGV;
  my $path = shift @ARGV;
  
  if ($style eq 'pretty') {
    $style = 0;
  } elsif ($style eq 'base64') {
    $style = 1;
  } else {
    die "Unrecognized style '$style'";
  }
  
  do_astral($path, $style);
  
} else {
  die "Unknown script mode '$script_mode'";
}

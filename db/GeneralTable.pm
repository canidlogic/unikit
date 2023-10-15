package GeneralTable;
use v5.16;
use warnings;

use Carp;

=head1 NAME

GeneralTable - Parser for the general UnicodeData data table file.

=head1 SYNOPSIS

  use GeneralTable;
  
  # Create a new parser on a given path
  my $gen = GeneralTable->parse($path);
  
  # Iterate over parsed records
  for(my $rec = $gen->readRec; defined $rec; $rec = $gen->readRec) {
    my $codepoint_lower  = $rec->{'lbound'};
    my $codepoint_upper  = $rec->{'ubound'};
    my $general_category = $rec->{'gencat'};
    my $combining_class  = $rec->{'cclass'};
    if (defined $rec->{'decomp'}) {        
      my $decomposition_string = $rec->{'decomp'};
      my $is_compatibility     = $rec->{'compat'};
    }
  }

=head1 DESCRIPTION

Read through parsed records of the UnicodeData.txt data file.  Only the
fields relevant to Unikit are parsed by this class.

=cut

# ===============
# Local functions
# ===============

# rawRec(fh)
# ----------
#
# Read a raw record from the given file handle, either returning a hash
# reference, or undef if no further records.
#
# The hash reference has the same format as is returned by readRec(),
# except that there is a 'uc' property holding a single codepoint
# instead of the 'lbound' and 'ubound' range properties, and there is a
# 'range' property that is either 0 meaning no range indicated in name,
# 1 meaning start of range indicated in name, or -1 meaning end of range
# indicated in name.
#
# This function does not enforce the requirement that codepoints must be
# read in ascending order.
#
sub rawRec {
  # Get parameters
  ($#_ == 0) or die;
  my $fh = shift;
  
  # Read the first non-blank line, returning undef if no more lines
  my $ltext;
  for($ltext = readline($fh); defined $ltext; $ltext = readline($fh)) {
    chomp $ltext;
    $ltext =~ s/#.*//;
    $ltext =~ s/^\s+//;
    $ltext =~ s/\s+$//;
    
    if (length($ltext) > 0) {
      last;
    }
  }
  (defined $ltext) or return undef;
  
  # Split record into fields
  ($ltext =~ /^([^;]*);([^;]*);([^;]*);([^;]*);[^;]*;([^;]*);/) or
    die "Invalid record";
  
  my $ucode  = $1;
  my $uname  = $2;
  my $gencat = $3;
  my $cclass = $4;
  my $decomp = $5;
  
  # Parse codepoint field
  ($ucode =~ /^\s*([0-9A-Fa-f]{1,6})\s*$/) or
    die "Invalid codepoint field";
  
  $ucode = hex($1);
  (($ucode >= 0) and ($ucode <= 0x10FFFF)) or
    die "Codepoint out of range";
  
  # Use name to determine range property
  my $range = 0;
  if ($uname =~ /^\s*</) {
    if ($uname =~ /First\s*>\s*$/i) {
      $range = 1;
    } elsif ($uname =~ /Last\s*>\s*$/i) {
      $range = -1;
    }
  }
  
  # Parse general category
  ($gencat =~ /^\s*([A-Z][a-z])\s*/) or die "Invalid category";
  $gencat = $1;
  
  # Parse cclass
  ($cclass =~ /^\s*([0-9]{1,3})\s*$/) or die "Invalid cclass";
  $cclass = int($1);
  (($cclass >= 0) and ($cclass <= 255)) or die "cclass out of range";
  
  # Form record, except for decomposition information
  my %rec = (
    'uc'     => $ucode,
    'range'  => $range,
    'gencat' => $gencat,
    'cclass' => $cclass
  );
  
  # Add decomposition information if present
  $decomp =~ s/^\s+//;
  $decomp =~ s/\s+$//;
  
  if (length($decomp) > 0) {
    # Decomposition present, so start by extracting compatibility marker
    # if present and setting compat flag
    $rec{'compat'} = 0;
    if ($decomp =~ /^<[^>]*>(.*)$/) {
      $decomp = $1;
      $rec{'compat'} = 1;
    }
    
    # Trim start of string again and make sure not empty
    $decomp =~ s/^\s+//;

    (length($decomp) > 0) or die "Invalid decomposition";
    
    # Parse codepoint string
    my @cpa = split /\s+/, $decomp;
    for my $cv (@cpa) {
      ($cv =~ /^[0-9A-Fa-f]{1,6}$/) or die "Invalid decomposition";
      $cv = chr(hex($cv));
    }
    $decomp = join "", @cpa;
    
    # Add to properties
    $rec{'decomp'} = $decomp;
  }
  
  # Return record
  return \%rec;
}

=head1 CONSTRUCTORS

=over 4

=item B<parse(path)>

Create a new parser object, given the path to the UnicodeData.txt file.

=cut

sub parse {
  # Get parameters
  ($#_ == 1) or croak("Bad call");
  shift;
  
  my $path = shift;
  (not ref($path)) or croak("Bad parameter type");
  (-f $path) or croak("Can't find data file '$path'");
  
  # Open the file in raw binary mode
  open(my $fh, "< :raw", $path) or 
    die "Failed to open file '$path'";
  
  # Create new object
  my $self = { };
  bless($self);
  
  # '_fh' stores the file handle of the data file
  $self->{'_fh'} = $fh;
  
  # '_done' is 1 if iteration is complete, 0 if not yet complete
  $self->{'_done'} = 0;
  
  # '_pos' is the last codepoint that was present in a returned record,
  # or -1 if no records have been returned yet
  $self->{'_pos'} = -1;
  
  # Return new object
  return $self;
}

=back

=head1 DESTRUCTOR

Closes the open file handle before the parser is released.

=cut

sub DESTROY {
  # Get self
  ($#_ == 0) or croak("Bad call");
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or croak("Bad self");
  
  # Close file
  close($self->{'_fh'}) or carp("Failed to close file");
  $self->{'_fh'} = undef;
}

=head1 PUBLIC INSTANCE FUNCTIONS

=over 4

=item B<readRec()>

Read the next parsed record from the data file.

The return value is either a hash reference to a parsed record, or 
C<undef> if there are no more records in the file.

The hash record always has C<lbound> and C<ubound> properties that
indicate the minimum and maximum codepoints that this record covers.  In
most records, these are the same (indicating a single codepoint), but in
some records, there is a range of codepoints, in which case C<ubound> is
greater than C<lbound>.  Both these properties are always integer values
in range 0x0000 to 0x10ffff.

The hash record always has a C<gencat> property that gives the General
Category.  This is always a string of two letters, where the first is an
uppercase ASCII letter and the second is a lowercase ASCII letter.

The hash record always has a C<cclass> property that gives the combining
class of the codepoint.  This is always an integer in range 0 to 254.

If the record has a canonical decomposition, then the C<decomp> and
C<compat> fields will both be present.  C<decomp> is a Unicode string
that stores the codepoints in the decomposition, while C<compat> is an
integer that is zero if this is a canonical decomposition or one if this
is a compatibility decomposition.

If there is no canonical decomposition, then neither C<decomp> nor
C<compat> will be defined.

The C<decomp> and C<compat> fields are only allowed to be defined when
C<lbound> and C<ubound> are equal.  In other words, decompositions are
not allowed for ranges of multiple codepoints.

The parsed records are guaranteed to be in ascending order of codepoint,
such that each record after the first has an C<lbound> that is greater
than the previous record's C<ubound>.

=cut

sub readRec {
  # Get self
  ($#_ == 0) or croak("Bad call");
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or croak("Bad self");
  
  # Return undef if iteration is finished
  ($self->{'_done'} == 0) or return undef;
  
  # Read a raw record
  my $rec = rawRec($self->{'_fh'});
  unless (defined $rec) {
    $self->{'_done'} = 1;
    return undef;
  }
  
  # Process ranges
  if ($rec->{'range'} > 0) {
    # Start of range, so get the next record
    my $rec2 = rawRec($self->{'_fh'});
    (defined $rec2) or die "Unclosed range";
    
    # Make sure no decomposition
    ((not defined $rec->{'decomp'}) and (not defined $rec2->{'decomp'}))
      or die "Decompositions not allowed in ranges";
    
    # Make sure gencat and cclass are equal
    (($rec->{'gencat'} eq $rec2->{'gencat'}) and
      ($rec->{'cclass'} == $rec2->{'cclass'})) or
        die "Parameter mismatch in range";
    
    # Make sure second record has greater codepoint than first
    ($rec2->{'uc'} > $rec->{'uc'}) or
      die "Invalid range boundaries";
    
    # Add range to first record
    $rec->{'lbound'} = $rec->{'uc'};
    $rec->{'ubound'} = $rec2->{'uc'};
    
    delete $rec->{'range'};
    delete $rec->{'uc'};
    
  } elsif ($rec->{'range'} == 0) {
    # No range
    $rec->{'lbound'} = $rec->{'uc'};
    $rec->{'ubound'} = $rec->{'uc'};
    
    delete $rec->{'range'};
    delete $rec->{'uc'};
    
  } elsif ($rec->{'range'} < 0) {
    die "Improper range";
    
  } else {
    die;
  }
  
  # Make sure in sequential order and update sequence information
  ($rec->{'lbound'} > $self->{'_pos'}) or
    die "Records out of order";
  $self->{'_pos'} = $rec->{'ubound'};
  
  # Return the record
  return $rec;
}

=back

=cut

# End with something that evaluates to true
1;

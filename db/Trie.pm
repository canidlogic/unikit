package Trie;
use v5.16;
use warnings;

use Carp;
use Scalar::Util qw(looks_like_number);

=head1 NAME

Trie - Trie data structure implementation for Unikit.

=head1 SYNOPSIS

  use Trie;
  
  # Create a new trie of depth 4
  my $tr = Trie->create(4);
  
  # Map a key to a specific value
  $tr->add([0, 1, 3, 7], $val);
  
  # Get the compiled trie array
  my @arr = $tr->compile;

=head1 DESCRIPTION

Tries are used for efficiently storing Unicode data.  Tries use a
fixed-length key that can be configured during construction to any
length between 1 and 8 nybbles.  The values stored in the trie may be
any unsigned 16-bit integer value except for 0xFFFF.

During compilation, tries generate an array consisting only of unsigned
16-bit integer values.  This array is divided into a sequence of tables,
where each table contains 16 integer elements.  Queries against this
table should always begin with the first table, which is the first 16
integers in the array.

In each table, one of the nybbles from the key is used as an index into
the table to select one of the 16 records.  For all tables, the special
value 0xFFFF means that the particular record is empty and unassigned.

Each query consults N tables, where N is the number of nybbles in the
key.  The first nybble is always used to query the first table.  For all
tables but the last in the query, the integer value selects a specific
table number within the compiled array, where table zero is the first
table in the array.  For the last table in the query, the value is the
actual value the key maps to, provided it is not the special value
0xFFFF.

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

# assignID(tptr, next_id)
# -----------------------
#
# Recursively assign unique ID values to each table in use by adding id
# properties.
#
# tptr is the table to recursively assign.  The table and all tables
# referenced from it will get their IDs assigned.
#
# next_id is the next ID to assign to a table.  It should be zero on the
# first call to assign zero to the first table.  It must be an integer
# in range 0 to 0xFFFE.
#
# The return value is the updated next_id that should be used for the
# next table.  It may be 0xFFFF if all available IDs have been assigned,
# which will cause a range error on the next attempt to assign.
#
# The next_id value returned from the root call where next_id was zero
# will be the total number of defined tables.
#
sub assignID {
  # Get parameters
  ($#_ == 1) or die "Bad call";
  
  my $tptr = shift;
  (ref($tptr) eq 'HASH') or die "Bad parameter type";
  
  my $next_id = shift;
  isInteger($next_id) or die "Bad parameter type";
  ($next_id >= 0) or die "Bad ID value";
  ($next_id <= 0xFFFE) or die "Too many tables in 16-bit trie";
  
  # Current table gets the given ID value
  $tptr->{'id'} = $next_id;
  $next_id++;
  
  # There should be a t key mapped to an array
  (defined $tptr->{'t'}) or die;
  (ref($tptr->{'t'}) eq 'ARRAY') or die;
  
  # Go through the array and recursively assign IDs to all defined hash
  # references in that array
  for my $e (@{$tptr->{'t'}}) {
    (defined $e) or next;
    (ref($e) eq 'HASH') or next;
    $next_id = assignID($e, $next_id);
  }
  
  # Return updated ID count
  return $next_id;
}

# genTable(\@result, tptr)
# ------------------------
#
# Recursively compile the given table and all tables referenced from it
# into the given result array.
#
# Before calling this on the root table, result should be allocated to
# the proper size and all elements initialized to 0xFFFF.
#
# Also, assignID must have already been used to assign IDs to each
# table.
#
sub genTable {
  # Get parameters
  ($#_ == 1) or die "Bad call";
  
  my $result = shift;
  (ref($result) eq 'ARRAY') or die "Bad parameter type";
  
  my $tptr = shift;
  (ref($tptr) eq 'HASH') or die "Bad parameter type";
  
  # Get the ID of this table
  (defined $tptr->{'id'}) or die "Unassigned table ID";
  
  my $tid = $tptr->{'id'};
  isInteger($tid) or die;
  (($tid >= 0) and ($tid <= 0xFFFE)) or die;
  
  # Compute the starting offset of the table in the array
  my $offs = $tid * 16;
  
  # Make sure the whole table is in result range
  (scalar(@$result) >= $offs + 16) or die "Wrong result size";
  
  # Make sure the t value resolves to an array of length 16
  (defined $tptr->{'t'}) or die;
  (ref($tptr->{'t'}) eq 'ARRAY') or die;
  (scalar(@{$tptr->{'t'}}) == 16) or die;
  
  # Write all defined array elements to the result array
  for(my $i = 0; $i < 16; $i++) {
    # Get element
    my $e = $tptr->{'t'}->[$i];
    
    # Skip undefined elements
    (defined $e) or next;
    
    # Handle different element types
    if (not ref($e)) {
      # Scalar element should be integer in range 0 to 0xFFFE
      isInteger($e) or die;
      (($e >= 0) and ($e <= 0xFFFE)) or die;
      
      # Store scalar element in result
      $result->[$offs + $i] = $e;
      
    } elsif (ref($e) eq 'HASH') {
      # Hash reference element should have an ID property
      (defined $e->{'id'}) or die;
      
      # Get ID and check that integer in range 0 to 0xFFFE
      my $sid = $e->{'id'};
      isInteger($sid) or die;
      (($sid >= 0) and ($sid <= 0xFFFE)) or die;
      
      # Store ID in result
      $result->[$offs + $i] = $sid;
      
    } else {
      die;
    }
  }
  
  # Recursively generate all referenced tables
  for(my $i = 0; $i < 16; $i++) {
    # Get element
    my $e = $tptr->{'t'}->[$i];
    
    # Skip undefined elements
    (defined $e) or next;
    
    # Skip elements except hash references
    (ref($e) eq 'HASH') or next;
    
    # Recursively generate table
    genTable($result, $e);
  }
}

=head1 CONSTRUCTORS

=over 4

=item B<create(depth)>

Create a new trie object.  The given depth must be an integer in range
1 to 8, which selects how many nybbles are in each key. 

=cut

sub create {
  # Get parameters
  ($#_ == 1) or croak("Bad call");
  shift;
  
  my $depth = shift;
  isInteger($depth) or croak("Invalid parameter type");
  (($depth >= 1) and ($depth <= 8)) or croak("Depth out of range");
  
  # Create new object
  my $self = { };
  bless($self);
  
  # '_depth' stores the depth of the trie
  $self->{'_depth'} = $depth;
  
  # '_done' is 1 if compiled, 0 if not yet compiled
  $self->{'_done'} = 0;
  
  # '_q' is a hash reference to the first table
  #
  # Each table is a hash that has a 't' key mapped to a subarray 
  # reference of exactly 16 elements.  The table starts out with all
  # elements at undef.
  #
  # All but the tables at maximum depth have defined elements that are
  # hash references to other tables of the same format.  Tables at the
  # maximum depth have defined values that are integers in range 0 to
  # 0xFFFE.
  #
  # During compilation, each table will get an 'id' key that is the 
  # unique index of the table that is in range zero or 0xFFFE.
  #
  $self->{'_q'} = {
    't' => [
      undef, undef, undef, undef,
      undef, undef, undef, undef,
      undef, undef, undef, undef,
      undef, undef, undef, undef
    ]
  };
  
  # Return new object
  return $self;
}

=back

=head1 PUBLIC INSTANCE FUNCTIONS

=over 4

=item B<add(key, val)>

Add another mapping to the trie.

key is the key of the mapping.  It must be an array reference, and the
array must have the same length as the depth that was used in the
constructor.  Each element of the array must be an integer in range 0 to
15, specifying a nybble value.

val is the value that the key is mapped to.  It must be an integer in
range 0 to 0xFFFE.  An error occurs if the mapping already exists.

=cut

sub add {
  # Get self
  ($#_ >= 0) or croak("Bad call");
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or croak("Bad self");
  
  # Check state
  ($self->{'_done'} == 0) or croak("Bad object state");
  
  # Get parameters
  ($#_ == 1) or croak("Bad call");
  
  my $key = shift;
  (ref($key) eq 'ARRAY') or croak("Bad parameter type");
  (scalar(@$key) == $self->{'_depth'}) or croak("Bad key length");
  for my $v (@$key) {
    isInteger($v) or croak("Bad key element type");
    (($v >= 0) and ($v <= 15)) or croak("Key element out of range");
  }
  
  my $val = shift;
  isInteger($val) or croak("Bad parameter type");
  (($val >= 0) and ($val <= 0xfffe)) or croak("Value out of range");
  
  # Start table pointer out at the first table
  my $tptr = $self->{'_q'};
  
  # For all key elements but the last, adjust the table pointer and
  # define any tables along that way that are not defined yet
  for(my $i = 0; $i <= $self->{'_depth'} - 2; $i++) {
    # Get current key element
    my $ke = $key->[$i];
    
    # Define new table if necessary
    unless (defined $tptr->{'t'}->[$ke]) {
      $tptr->{'t'}->[$ke] = {
        't' => [
          undef, undef, undef, undef,
          undef, undef, undef, undef,
          undef, undef, undef, undef,
          undef, undef, undef, undef
        ]
      };
    }
    
    # Move table pointer to new table
    $tptr = $tptr->{'t'}->[$ke];
  }
  
  # Determine the last key element and make sure it is not yet defined
  my $kl = $key->[-1];
  (not defined $tptr->{'t'}->[$kl]) or croak("Key already defined");
  
  # Store the value
  $tptr->{'t'}->[$kl] = $val;
}

=item B<compile()>

Compile the trie to an array of unsigned 16-bit values.

The return is the compiled array in list context.  It is never empty and
its length is always a multiple of 16.

See the documentation at the top of this module for the format of the
array and how to query it.

No further calls can be made to the object after it has been compiled.

=cut

sub compile {
  # Get self
  ($#_ >= 0) or croak("Bad call");
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or croak("Bad self");
  
  # Check state
  ($self->{'_done'} == 0) or croak("Bad object state");
  
  # Check parameters
  ($#_ < 0) or croak("Bad call");
  
  # Update state
  $self->{'_done'} = 1;
  
  # Recursively assign IDs and get the full table count
  my $table_count = assignID($self->{'_q'}, 0);
  
  # Define the result array with all tables initially set to 0xFFFF
  # values indicating no record present
  my @result;
  for(my $i = 0; $i < $table_count; $i++) {
    push @result, (
                    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
                    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
                    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
                    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF
                  );
  }
  
  # Recursively generate all tables
  genTable(\@result, $self->{'_q'});
  
  # Clear internal data structures
  $self->{'_q'} = undef;
  
  # Return results
  return @result;
}

=back

=cut

# End with something that evaluates to true
1;

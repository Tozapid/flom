#
# Copyright (c) 2013-2016, Christian Ferrari <tiian@users.sourceforge.net>
# All rights reserved.
#
# This file is part of FLoM.
#
# FLoM is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as published
# by the Free Software Foundation.
#
# FLoM is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with FLoM.  If not, see <http://www.gnu.org/licenses/>.
#


use strict;
use warnings;

use Flom;

my $ret_cod = Flom::RC_OK;
my $handle;

# step 1: handle creation
unless ($handle = Flom::handle_new()) {
    printf(STDERR "Flom::handle_new() returned %p\n", $handle);
    exit 1;
}

# step 2: lock acquisition
if (Flom::RC_OK != ($ret_cod = Flom::handle_lock($handle))) {
    printf(STDERR "Flom::handle_lock() returned %d, %s\n",
	   $ret_cod, Flom::strerror($ret_cod));
    exit 1;
}

# step 3: execute the code that needs lock protection

# step 4: lock release
if (Flom::RC_OK != ($ret_cod = Flom::handle_unlock($handle))) {
    printf(STDERR "Flom::handle_unlock() returned %d, %s\n",
	   $ret_cod, Flom::strerror($ret_cod));
    exit 1;
}

# step 5: delete the handle
Flom::handle_delete($handle);

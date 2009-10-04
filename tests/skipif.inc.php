<?php

// Please not that databases defined in config.inc.php
// will be emptied during the process. Make sure that 
// they don't point to important databases
//die('skip see tests/skipif.inc.php');

if (!extension_loaded('tokyo_tyrant'))
	die('skip no tokyo_tyrant extension loaded');
?>

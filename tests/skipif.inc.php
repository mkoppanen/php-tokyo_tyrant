<?php

// Please note that databases defined in config.inc.php
// will be emptied during the process. Make sure that 
// they don't point to important databases
if (!file_exists(dirname(__FILE__) . '/noskip.local')) {
    die('skip see tests/skipif.inc.php');
}

if (!extension_loaded('tokyo_tyrant'))
	die('skip no tokyo_tyrant extension loaded');
?>

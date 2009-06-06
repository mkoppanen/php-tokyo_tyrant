<?php
if (!extension_loaded('tokyo_tyrant'))
	die('Skip');

function get_tt_type() {
	$tt = new TokyoTyrant('localhost');
	$string = $tt->stat();

	foreach (explode("\n", $string) as $line) {
		$parts = explode("\t", $line);

		if ($parts[0] == 'type') {
			return $parts[1];
		}
	}
}

function skip_if_table() {
	if (get_tt_type() == 'table')
		die('Skip. Not for table database');
}

function skip_if_not_table() {
	if (get_tt_type() != 'table')
		die('Skip. Only for table database');
}


?>
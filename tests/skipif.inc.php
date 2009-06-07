<?php
if (!extension_loaded('tokyo_tyrant'))
	die('Skip');

function skip_if_table() {
	
	$tt = new TokyoTyrant(TT_HOST, TT_PORT);
	$arr = $tt->stat();
	
	if ($arr['type'] == 'table')
		die('Skip. Not for table database');
}

function skip_if_not_table() {
	$tt = new TokyoTyrant(TT_HOST, TT_PORT);
	$arr = $tt->stat();
	
	if ($arr['type'] != 'table')
		die('Skip. Only for table database');
}


?>
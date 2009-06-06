<?php

$values = array("a" => "a.a", "b" => "b.b", "c" => "c.c", 1 => 2111);

/* Connect to a table database */
$table = new TokyoTyrant(); 
$table->connect("localhost", 1980);
$table->tablePut($values);

$query = $table->getquery();

/* Get records through iterator */
foreach ($query as $k => $row) {
	var_dump($k, $row);
}
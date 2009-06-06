<?php
$values = array("a" => "a.a", "b" => "b.b", "c" => "c.c", 1 => 2111);

$table = new TokyoTyrant(); 
$table->connect("localhost", 1980);

$query  = $table->getquery();
$values = $query->search();

var_dump($values);

--TEST--
Test add int and double
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc.php";
skip_if_table();
?>
--FILE--
<?php
include 'config.inc.php';

$tt = new TokyoTyrant(TT_HOST, TT_PORT);
$tt->vanish();

var_dump($tt->add('test_int', 3));
var_dump($tt->add('test_int', 6));

echo "---------------\n";

var_dump($tt->add('test_double', 3.00));
var_dump($tt->add('test_double', 3.5));
var_dump($tt->add('test_double', 3.5));
var_dump($tt->add('test_double', 3.5));

$tt->vanish();

?>
--EXPECT--
int(3)
int(9)
---------------
float(3)
float(6.5)
float(10)
float(13.5)
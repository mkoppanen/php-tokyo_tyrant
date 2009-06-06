--TEST--
Test add int and double
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc.php";
skip_if_not_table();
?>
--FILE--
<?php
include 'config.inc.php';

$tt = new TokyoTyrant(TT_HOST, TT_PORT);
$tt->vanish();

$rec = $tt->tableput(array('test' => 'data', 'something' => time()));
var_dump($rec);

$query = $tt->getQuery();






$tt->vanish();

?>
--EXPECTF--
int(%d)
string(4) "test"
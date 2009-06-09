--TEST--
Test number of records
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc.php";
?>
--FILE--
<?php
include 'config.inc.php';

$tt = new TokyoTyrant(TT_HOST, TT_PORT);
$tt->vanish();

$tt->put('test_key1', 'data');
var_dump($tt->num());
$tt->put('test_key2', 'data');
var_dump($tt->num());
$tt->put('test_key3', 'data');
var_dump($tt->num());
$tt->put('test_key4', 'data');
var_dump($tt->num());
$tt->put('test_key5', 'data');
var_dump($tt->num());

$tt->vanish();
var_dump($tt->num());
?>
--EXPECT--
int(1)
int(2)
int(3)
int(4)
int(5)
int(0)
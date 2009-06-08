--TEST--
Test forward matching keys
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc.php";
?>
--FILE--
<?php
include 'config.inc.php';

$tt = new TokyoTyrant(TT_HOST, TT_PORT);
$tt->vanish();

$tt->put('test_key1', 'data12');
$tt->put('test_key2', 'data34');
$tt->put('test_key3', 'data56');
$tt->put('test_key4', 'data78');
$tt->put('test_key5', 'data90');

/* 5, 1, 3 */
var_dump(count($tt->fwmkeys('test', 10)));
var_dump(count($tt->fwmkeys('test', 1)));
var_dump(count($tt->fwmkeys('test', 3)));

/* rand */
var_dump(count($tt->fwmkeys('test', -1233)));

/* 0 */
var_dump(count($tt->fwmkeys('123t123123est', 3)));
?>
--EXPECTF--
int(5)
int(1)
int(3)
int(%d)
int(0)
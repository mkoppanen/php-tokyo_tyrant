--TEST--
Test size
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc";
?>
--FILE--
<?php
include dirname(__FILE__) . '/config.inc';

$tt = new TokyoTyrant(TT_HOST, TT_PORT);
$tt->vanish();

$tt->put('test_key', 'data');
var_dump($tt->size('test_key'));

$tt->vanish();
var_dump($tt->size('test_key'));

?>
--EXPECT--
int(4)
NULL
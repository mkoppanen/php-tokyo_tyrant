--TEST--
Make sure that stat returns an array
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc.php";
?>
--FILE--
<?php
include 'config.inc.php';

$tt = new TokyoTyrant(TT_HOST, TT_PORT);
echo gettype($tt->stat());


?>
--EXPECT--
array
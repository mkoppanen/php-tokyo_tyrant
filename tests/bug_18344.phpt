--TEST--
Test for PECL bug #18344
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc";
?>
--FILE--
<?php
include dirname(__FILE__) . '/config.inc';

$tt = new TokyoTyrant(TT_HOST, TT_PORT);
$tt->vanish();

$key = 1;
$tt->put($key, 'value');
echo gettype($key) , "\n";

echo "Done\n";

?>
--EXPECT--
integer
Done
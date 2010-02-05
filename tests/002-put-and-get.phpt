--TEST--
Test single value put / get variations
--SKIPIF--
<?php
include dirname(__FILE__) . "/skipif.inc.php";
?>
--FILE--
<?php
include 'config.inc.php';

$tt = new TokyoTyrant(TT_HOST, TT_PORT);
$tt->vanish();

$tt->put('test_key', 'data');
echo $tt->get('test_key') . "\n";

$tt->putcat('test_key', ' more data');
echo $tt->get('test_key') . "\n";

try {
	$tt->putkeep('test_key', 'xxxxx');
	echo "putkeep succeeded. the key should exist\n";
} catch (TokyoTyrantException $e) {
	echo "got exception\n";
}

$tt->put("test", "abc"); 
$tt->putshl("test", "de", 4); 
echo $tt->get("test") . "\n";

$tt->vanish();
$tt->putkeep('test_key', 'xxxxx');
echo $tt->get('test_key') . "\n";
?>
--EXPECT--
data
data more data
got exception
bcde
xxxxx

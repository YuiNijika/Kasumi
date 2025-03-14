<?php 
if (!defined('__TYPECHO_ROOT_DIR__')) exit;

// 获取 TranslationLang 设置项
$translationLang = Get::Options('TranslationLang');

// 定义所有可用的语言选项
$allLanguages = [
    'chinese_simplified' => '简体中文',
    'chinese_traditional' => '繁体中文',
    'english' => 'English',
    'japanese' => '日本語',
    'korean' => '한국어',
    'ukrainian' => 'УкраїнськаName',
    'norwegian' => 'Norge',
    'welsh' => 'color name',
    'dutch' => 'nederlands',
    'filipino' => 'Pilipino',
    'lao' => 'ກະຣຸນາ',
    'telugu' => 'తెలుగుQFontDatabase',
    'romanian' => 'Română',
    'nepali' => 'नेपालीName',
    'french' => 'Français',
    'haitian_creole' => 'Kreyòl ayisyen',
    'czech' => 'český',
    'swedish' => 'Svenska',
    'russian' => 'Русский язык',
    'malagasy' => 'Malagasy',
    'burmese' => 'ဗာရမ်',
    'pashto' => 'پښتوName',
    'thai' => 'คนไทย',
    'armenian' => 'Արմենյան',
    'persian' => 'Persian',
    'kurdish' => 'Kurdî',
    'turkish' => 'Türkçe',
    'hindi' => 'हिन्दी',
    'bulgarian' => 'български',
    'malay' => 'Malay',
    'swahili' => 'Kiswahili',
    'oriya' => 'ଓଡିଆ',
    'icelandic' => 'ÍslandName',
    'irish' => 'Íris',
    'khmer' => 'ខ្មែរKCharselect unicode block name',
    'gujarati' => 'ગુજરાતી',
    'slovak' => 'Slovenská',
    'kannada' => 'ಕನ್ನಡ್Name',
    'hebrew' => 'היברית',
    'hungarian' => 'magyar',
    'marathi' => 'मराठीName',
    'tamil' => 'தாமில்',
    'estonian' => 'eesti keel',
    'malayalam' => 'മലമാലം',
    'inuktitut' => 'ᐃᓄᒃᑎᑐᑦ',
    'arabic' => 'بالعربية',
    'deutsch' => 'Deutsch',
    'slovene' => 'slovenščina',
    'bengali' => 'বেঙ্গালী',
    'urdu' => 'اوردو',
    'azerbaijani' => 'azerbaijani',
    'portuguese' => 'português',
    'samoan' => 'lifiava',
    'afrikaans' => 'afrikaans',
    'tongan' => '汤加语',
    'greek' => 'ελληνικά',
    'indonesian' => 'IndonesiaName',
    'spanish' => 'Español',
    'danish' => 'dansk',
    'amharic' => 'amharic',
    'punjabi' => 'ਪੰਜਾਬੀName',
    'albanian' => 'albanian',
    'lithuanian' => 'Lietuva',
    'italian' => 'italiano',
    'vietnamese' => 'Tiếng Việt',
    'maltese' => 'Malti',
    'finnish' => 'suomi',
    'catalan' => 'català',
    'croatian' => 'hrvatski',
    'bosnian' => 'bosnian',
    'polish' => 'Polski',
    'latvian' => 'latviešu',
    'maori' => 'Maori',
];

// 确保 $translationLang 是一个数组
if (!is_array($translationLang)) {
    $translationLang = [];
}

// 过滤出选中的语言
$languages = array_filter($allLanguages, function($code) use ($translationLang) {
    return in_array($code, $translationLang);
}, ARRAY_FILTER_USE_KEY);

// 生成语言选项
foreach ($languages as $code => $name) { ?>
    <a href="javascript:translate.changeLanguage('<?php echo $code; ?>');" class="mdui-list-item mdui-ripple" role="menuitem">
        <div class="mdui-list-item-content"><?php echo $name; ?></div>
    </a>
<?php }; ?>
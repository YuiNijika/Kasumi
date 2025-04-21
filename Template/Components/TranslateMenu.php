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
$languages = array_filter($allLanguages, function ($code) use ($translationLang) {
    return in_array($code, $translationLang);
}, ARRAY_FILTER_USE_KEY);
?>
<ul class="mdui-list" mdui-collapse="{accordion: true}">

    <li class="mdui-collapse-item">
        <div class="mdui-collapse-item-header mdui-list-item mdui-ripple">
            <i class="mdui-list-item-icon mdui-icon material-icons">language</i>
            <div class="mdui-list-item-content">页面翻译</div>
            <i class="mdui-collapse-item-arrow mdui-icon material-icons">keyboard_arrow_down</i>
        </div>
        <ul class="mdui-collapse-item-body mdui-list mdui-list-dense">
            <?php
            // 生成语言选项
            foreach ($languages as $code => $name) { ?>
                <a href="javascript:translate.changeLanguage('<?php echo $code; ?>');" role="menuitem">
                    <li class="mdui-list-item mdui-ripple"><?php echo $name ?></li>
                </a>
            <?php }; ?>
        </ul>
    </li>
</ul>
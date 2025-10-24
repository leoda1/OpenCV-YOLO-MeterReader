#include "helpdialog.h"
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QDialogButtonBox>

HelpDialog::HelpDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("帮助说明");
    resize(900, 650);
    buildUi();
}

void HelpDialog::buildUi() {
    auto* layout = new QVBoxLayout(this);

    m_browser = new QTextBrowser(this);
    m_browser->setOpenExternalLinks(true);
    m_browser->setHtml(buildHtml());
    m_browser->setStyleSheet(R"(
        QTextBrowser {
            background: #fafafa;
            border: none;
            padding: 8px 12px;
        }
        h1 { color: #2E86C1; font-size: 26px; margin: 0 0 10px 0; }
        h2 { color: #34495E; font-size: 20px; margin: 18px 0 8px 0; }
        h3 { color: #2C3E50; font-size: 16px; margin: 14px 0 6px 0; }
        p  { font-size: 14px; line-height: 1.6; color: #333; }
        li { font-size: 14px; line-height: 1.6; color: #333; margin: 4px 0; }
        .hint { color: #16A085; font-weight: bold; }
        .warn { color: #E74C3C; font-weight: bold; }
        .note { color: #7F8C8D; }
        .box  { background:#fff; border:1px solid #e3e3e3; border-radius:8px; padding:12px 14px; }
        .kbd  { font-family: Consolas, monospace; background:#eee; padding:1px 6px; border-radius:4px; }
        hr { border: none; border-top: 1px dashed #ddd; margin: 14px 0; }
    )");
    layout->addWidget(m_browser, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &HelpDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &HelpDialog::accept);
    layout->addWidget(buttons, 0);
}

QString HelpDialog::buildHtml() const {
    // 内容按您的要求组织为清晰的章节和要点
    return R"(
<h1>帮助说明</h1>

<div class='box'>
  <h2>采集环境</h2>
  <ul>
    <li>必须有足够的高光照条件，均匀照射表盘。</li>
    <li>避免表盘出现阴影、反光或局部过曝。</li>
  </ul>
  <p class='note'>建议：保证镜头清洁、表盘平直于镜头，减少畸变。</p>
</div>

<hr/>

<div class='box'>
  <h2>采集结果要求</h2>
  <p><b>YYQY-13</b> 型号：绿色定位点与红色检测线<b>必须位于指针正中</b>，否则应调整光照或相机位姿后重试。</p>
</div>

<hr/>

<div class='box'>
  <h2>采集操作流程</h2>
  <ol>
    <li><span class='kbd'>归位</span>：先点击“归位”设定零位初始起点。</li>
    <li><span class='kbd'>采集</span>：点击“采集”，检测结果会显示在图片左上角；随后点击“确认”保存该角度。</li>
    <li><span class='kbd'>最大角度采集</span>：达到最大角度时点击“大角度采”，再点“确认”保存最大角度。</li>
    <li><span class='kbd'>归位/清空</span>：
        <ul>
          <li>采集后点击“归位”会<b>删除当前轮次数据</b>。</li>
          <li>点击“清空”会<b>删除所有轮次数据</b>。</li>
        </ul>
    </li>
    <li><span class='kbd'>保存轮次</span>：一轮数据完成后点击“保存”，会保存本轮数据并自动跳转到下一轮。</li>
  </ol>
  <p class='note'>状态栏会实时提示当前轮次、行程与采集进度；误差表窗口将同步显示统计与分析。</p>
</div>

<hr/>

<div class='box'>
  <h2>侧边栏按钮</h2>
  <ul>
    <li>从上到下：<b>保存 / 归位 / 采集 / 确认 / 刷新</b>（功能同右侧对应按钮）。</li>
    <li><b>设置</b>：显示/调整相机参数，<b>可修改采集轮次</b>。</li>
    <li><b>帮助</b>：显示本帮助说明与注意事项。</li>
  </ul>
</div>

<hr/>

<div class='box'>
  <h2>绘制表盘</h2>
  <ol>
    <li>点击“绘制表盘”后，左侧显示初始化的表盘图片。</li>
    <li>在图片上<b>右键</b>可新建文字标注，自定义内容。</li>
    <li>右侧面板可调整文字内容、颜色、字体、大小，支持选择标注<b>删除/保存/加载</b>。</li>
    <li>右下方显示<b>当前图片角度</b>；可<b>保存</b>当前生成的表盘。</li>
    <li><span class='hint'>绿色按钮</span>：从已打开的<b>误差分析</b>窗口读取数据，<b>自动生成新表盘</b>。</li>
  </ol>
</div>

<hr/>

<div class='box'>
  <h2>误差分析</h2>
  <ul>
    <li>点击进入后，显示各检测点的<b>采集角度</b>与<b>分析结果</b>。</li>
    <li>左下角“误差分析结果”会逐轮展示：每个检测点的<b>误差</b>与<b>迟滞误差</b>是否超标，以及<b>最终结论</b>。</li>
    <li>右下角按钮可将检测数据<b>导出为 Excel (CSV)</b>。</li>
  </ul>
  <p class='note'>分析区在录入或导入历史数据后会自动刷新，便于快速预检与终检。</p>
</div>
)";
}
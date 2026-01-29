declare module 'react-plotly.js' {
  import * as Plotly from 'plotly.js';
  import * as React from 'react';

  interface PlotParams {
    data: Plotly.Data[];
    layout?: Partial<Plotly.Layout>;
    frames?: Plotly.Frame[];
    config?: Partial<Plotly.Config>;
    style?: React.CSSProperties;
    className?: string;
    useResizeHandler?: boolean;
    divId?: string;
    onInitialized?: (figure: Readonly<Plotly.Figure>, graphDiv: Readonly<HTMLDivElement>) => void;
    onUpdate?: (figure: Readonly<Plotly.Figure>, graphDiv: Readonly<HTMLDivElement>) => void;
    onPurge?: (figure: Readonly<Plotly.Figure>, graphDiv: Readonly<HTMLDivElement>) => void;
    onError?: (err: Error) => void;
    onAfterExport?: () => void;
    onAfterPlot?: () => void;
    onAnimated?: () => void;
    onAnimatingFrame?: (event: Plotly.FrameAnimationEvent) => void;
    onAnimationInterrupted?: () => void;
    onAutoSize?: () => void;
    onBeforeExport?: () => void;
    onBeforeHover?: (event: Plotly.PlotMouseEvent) => boolean;
    onButtonClicked?: (event: Plotly.ButtonClickedEvent) => void;
    onClick?: (event: Plotly.PlotMouseEvent) => void;
    onClickAnnotation?: (event: Plotly.ClickAnnotationEvent) => void;
    onDeselect?: () => void;
    onDoubleClick?: () => void;
    onFramework?: () => void;
    onHover?: (event: Plotly.PlotHoverEvent) => void;
    onLegendClick?: (event: Plotly.LegendClickEvent) => boolean;
    onLegendDoubleClick?: (event: Plotly.LegendClickEvent) => boolean;
    onRelayout?: (event: Plotly.PlotRelayoutEvent) => void;
    onRestyle?: (event: Plotly.PlotRestyleEvent) => void;
    onRedraw?: () => void;
    onSelected?: (event: Plotly.PlotSelectionEvent) => void;
    onSelecting?: (event: Plotly.PlotSelectionEvent) => void;
    onSliderChange?: (event: Plotly.SliderChangeEvent) => void;
    onSliderEnd?: (event: Plotly.SliderEndEvent) => void;
    onSliderStart?: (event: Plotly.SliderStartEvent) => void;
    onTransitioning?: () => void;
    onTransitionInterrupted?: () => void;
    onUnhover?: (event: Plotly.PlotMouseEvent) => void;
    onWebGLContextLost?: () => void;
  }

  class Plot extends React.Component<PlotParams> {}

  export default Plot;
}

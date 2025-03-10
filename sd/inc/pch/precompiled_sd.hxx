/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 This file has been autogenerated by update_pch.sh . It is possible to edit it
 manually (such as when an include file has been moved/renamed/removed. All such
 manual changes will be rewritten by the next run of update_pch.sh (which presumably
 also fixes all possible problems, so it's usually better to use it).
*/

#include "avmedia/mediawindow.hxx"
#include "boost/noncopyable.hpp"
#include "canvas/elapsedtime.hxx"
#include "com/sun/star/office/XAnnotationEnumeration.hpp"
#include "com/sun/star/uno/RuntimeException.hpp"
#include "com/sun/star/uno/XComponentContext.hpp"
#include "comphelper/anytostring.hxx"
#include "comphelper/scopeguard.hxx"
#include "cppuhelper/exc_hlp.hxx"
#include "cppuhelper/implbase1.hxx"
#include "osl/diagnose.h"
#include "osl/time.h"
#include "rtl/ref.hxx"
#include "sal/config.h"
#include "sal/log.hxx"
#include "sal/types.h"
#include "sfx2/viewfrm.hxx"
#include "svtools/colrdlg.hxx"
#include "svtools/slidesorterbaropt.hxx"
#include "svtools/svlbitm.hxx"
#include "svtools/toolpanelopt.hxx"
#include "svtools/treelistentry.hxx"
#include "svtools/viewdataentry.hxx"
#include "svx/xtable.hxx"
#include "vcl/canvastools.hxx"
#include "vcl/svapp.hxx"
#include <algorithm>
#include <animations/animationnodehelper.hxx>
#include <avmedia/mediaitem.hxx>
#include <avmedia/mediaplayer.hxx>
#include <avmedia/mediatoolbox.hxx>
#include <avmedia/mediawindow.hxx>
#include <avmedia/modeltools.hxx>
#include <basegfx/color/bcolor.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>
#include <basegfx/matrix/b2dhommatrixtools.hxx>
#include <basegfx/numeric/ftools.hxx>
#include <basegfx/point/b2dpoint.hxx>
#include <basegfx/polygon/b2dpolygon.hxx>
#include <basegfx/polygon/b2dpolygonclipper.hxx>
#include <basegfx/polygon/b2dpolygontools.hxx>
#include <basegfx/polygon/b2dpolypolygon.hxx>
#include <basegfx/polygon/b2dpolypolygontools.hxx>
#include <basegfx/range/b2drange.hxx>
#include <basegfx/range/b2drectangle.hxx>
#include <basegfx/tools/canvastools.hxx>
#include <basegfx/tools/tools.hxx>
#include <basegfx/tools/zoomtools.hxx>
#include <basegfx/tuple/b2dtuple.hxx>
#include <basic/basmgr.hxx>
#include <basic/sberrors.hxx>
#include <basic/sbstar.hxx>
#include <basic/sbx.hxx>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/foreach.hpp>
#include <boost/function.hpp>
#include <boost/limits.hpp>
#include <boost/make_shared.hpp>
#include <boost/noncopyable.hpp>
#include <boost/optional.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/scoped_array.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <boost/weak_ptr.hpp>
#include <canvas/canvastools.hxx>
#include <canvas/elapsedtime.hxx>
#include <com/sun/star/accessibility/AccessibleEventId.hpp>
#include <com/sun/star/accessibility/AccessibleRole.hpp>
#include <com/sun/star/accessibility/AccessibleStateType.hpp>
#include <com/sun/star/animations/AnimateColor.hpp>
#include <com/sun/star/animations/AnimateMotion.hpp>
#include <com/sun/star/animations/AnimateSet.hpp>
#include <com/sun/star/animations/AnimationFill.hpp>
#include <com/sun/star/animations/AnimationNodeType.hpp>
#include <com/sun/star/animations/AnimationRestart.hpp>
#include <com/sun/star/animations/AnimationTransformType.hpp>
#include <com/sun/star/animations/Audio.hpp>
#include <com/sun/star/animations/Command.hpp>
#include <com/sun/star/animations/Event.hpp>
#include <com/sun/star/animations/EventTrigger.hpp>
#include <com/sun/star/animations/IterateContainer.hpp>
#include <com/sun/star/animations/ParallelTimeContainer.hpp>
#include <com/sun/star/animations/SequenceTimeContainer.hpp>
#include <com/sun/star/animations/Timing.hpp>
#include <com/sun/star/animations/ValuePair.hpp>
#include <com/sun/star/animations/XAnimate.hpp>
#include <com/sun/star/animations/XAnimateColor.hpp>
#include <com/sun/star/animations/XAnimateMotion.hpp>
#include <com/sun/star/animations/XAnimateSet.hpp>
#include <com/sun/star/animations/XAnimateTransform.hpp>
#include <com/sun/star/animations/XAnimationNode.hpp>
#include <com/sun/star/animations/XAnimationNodeSupplier.hpp>
#include <com/sun/star/animations/XAudio.hpp>
#include <com/sun/star/animations/XCommand.hpp>
#include <com/sun/star/animations/XIterateContainer.hpp>
#include <com/sun/star/animations/XTimeContainer.hpp>
#include <com/sun/star/animations/XTransitionFilter.hpp>
#include <com/sun/star/awt/FontDescriptor.hpp>
#include <com/sun/star/awt/FontSlant.hpp>
#include <com/sun/star/awt/FontUnderline.hpp>
#include <com/sun/star/awt/FontWeight.hpp>
#include <com/sun/star/awt/Key.hpp>
#include <com/sun/star/awt/KeyModifier.hpp>
#include <com/sun/star/awt/Pointer.hpp>
#include <com/sun/star/awt/Rectangle.hpp>
#include <com/sun/star/awt/Size.hpp>
#include <com/sun/star/awt/SystemPointer.hpp>
#include <com/sun/star/awt/WindowAttribute.hpp>
#include <com/sun/star/awt/WindowClass.hpp>
#include <com/sun/star/awt/WindowDescriptor.hpp>
#include <com/sun/star/awt/XDevice.hpp>
#include <com/sun/star/awt/XWindow.hpp>
#include <com/sun/star/beans/NamedValue.hpp>
#include <com/sun/star/beans/PropertyAttribute.hpp>
#include <com/sun/star/beans/PropertyChangeEvent.hpp>
#include <com/sun/star/beans/PropertyState.hpp>
#include <com/sun/star/beans/PropertyValue.hpp>
#include <com/sun/star/beans/PropertyValues.hpp>
#include <com/sun/star/beans/UnknownPropertyException.hpp>
#include <com/sun/star/beans/XMultiPropertySet.hpp>
#include <com/sun/star/beans/XMultiPropertyStates.hpp>
#include <com/sun/star/beans/XPropertyAccess.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/beans/XPropertySetInfo.hpp>
#include <com/sun/star/chart2/XChartDocument.hpp>
#include <com/sun/star/configuration/theDefaultProvider.hpp>
#include <com/sun/star/container/XChild.hpp>
#include <com/sun/star/container/XEnumerationAccess.hpp>
#include <com/sun/star/container/XHierarchicalNameAccess.hpp>
#include <com/sun/star/container/XIndexAccess.hpp>
#include <com/sun/star/container/XNameAccess.hpp>
#include <com/sun/star/container/XNameContainer.hpp>
#include <com/sun/star/container/XNameReplace.hpp>
#include <com/sun/star/container/XNamed.hpp>
#include <com/sun/star/datatransfer/dnd/DNDConstants.hpp>
#include <com/sun/star/document/IndexedPropertyValues.hpp>
#include <com/sun/star/document/PrinterIndependentLayout.hpp>
#include <com/sun/star/document/XDocumentProperties.hpp>
#include <com/sun/star/document/XDocumentPropertiesSupplier.hpp>
#include <com/sun/star/document/XEventBroadcaster.hpp>
#include <com/sun/star/document/XEventsSupplier.hpp>
#include <com/sun/star/document/XExporter.hpp>
#include <com/sun/star/document/XFilter.hpp>
#include <com/sun/star/document/XGraphicObjectResolver.hpp>
#include <com/sun/star/document/XImporter.hpp>
#include <com/sun/star/document/XViewDataSupplier.hpp>
#include <com/sun/star/drawing/BitmapMode.hpp>
#include <com/sun/star/drawing/DrawViewMode.hpp>
#include <com/sun/star/drawing/FillStyle.hpp>
#include <com/sun/star/drawing/GraphicExportFilter.hpp>
#include <com/sun/star/drawing/GraphicFilterRequest.hpp>
#include <com/sun/star/drawing/LineStyle.hpp>
#include <com/sun/star/drawing/ShapeCollection.hpp>
#include <com/sun/star/drawing/XDrawPage.hpp>
#include <com/sun/star/drawing/XDrawPages.hpp>
#include <com/sun/star/drawing/XDrawPagesSupplier.hpp>
#include <com/sun/star/drawing/XDrawView.hpp>
#include <com/sun/star/drawing/XLayer.hpp>
#include <com/sun/star/drawing/XLayerManager.hpp>
#include <com/sun/star/drawing/XMasterPageTarget.hpp>
#include <com/sun/star/drawing/XMasterPagesSupplier.hpp>
#include <com/sun/star/drawing/XSelectionFunction.hpp>
#include <com/sun/star/drawing/XShape.hpp>
#include <com/sun/star/drawing/XShapeDescriptor.hpp>
#include <com/sun/star/drawing/XShapes.hpp>
#include <com/sun/star/drawing/framework/ConfigurationChangeEvent.hpp>
#include <com/sun/star/drawing/framework/ConfigurationController.hpp>
#include <com/sun/star/drawing/framework/ModuleController.hpp>
#include <com/sun/star/drawing/framework/ResourceActivationMode.hpp>
#include <com/sun/star/drawing/framework/ResourceId.hpp>
#include <com/sun/star/drawing/framework/TabBarButton.hpp>
#include <com/sun/star/drawing/framework/XConfiguration.hpp>
#include <com/sun/star/drawing/framework/XConfigurationChangeListener.hpp>
#include <com/sun/star/drawing/framework/XConfigurationController.hpp>
#include <com/sun/star/drawing/framework/XControllerManager.hpp>
#include <com/sun/star/drawing/framework/XPane.hpp>
#include <com/sun/star/drawing/framework/XTabBar.hpp>
#include <com/sun/star/drawing/framework/XView.hpp>
#include <com/sun/star/embed/Aspects.hpp>
#include <com/sun/star/embed/ElementModes.hpp>
#include <com/sun/star/embed/EmbedMisc.hpp>
#include <com/sun/star/embed/EmbedStates.hpp>
#include <com/sun/star/embed/MSOLEObjectSystemCreator.hpp>
#include <com/sun/star/embed/NoVisualAreaSizeException.hpp>
#include <com/sun/star/embed/StorageFactory.hpp>
#include <com/sun/star/embed/XEmbedObjectClipboardCreator.hpp>
#include <com/sun/star/embed/XEmbedPersist.hpp>
#include <com/sun/star/embed/XStorage.hpp>
#include <com/sun/star/embed/XTransactedObject.hpp>
#include <com/sun/star/embed/XVisualObject.hpp>
#include <com/sun/star/form/FormButtonType.hpp>
#include <com/sun/star/frame/Desktop.hpp>
#include <com/sun/star/frame/DispatchResultState.hpp>
#include <com/sun/star/frame/DocumentTemplates.hpp>
#include <com/sun/star/frame/FrameAction.hpp>
#include <com/sun/star/frame/FrameActionEvent.hpp>
#include <com/sun/star/frame/ModuleManager.hpp>
#include <com/sun/star/frame/UnknownModuleException.hpp>
#include <com/sun/star/frame/XComponentLoader.hpp>
#include <com/sun/star/frame/XController.hpp>
#include <com/sun/star/frame/XDispatch.hpp>
#include <com/sun/star/frame/XDispatchProvider.hpp>
#include <com/sun/star/frame/XDocumentTemplates.hpp>
#include <com/sun/star/frame/XFrame.hpp>
#include <com/sun/star/frame/XFramesSupplier.hpp>
#include <com/sun/star/frame/XLayoutManager.hpp>
#include <com/sun/star/frame/XModel.hpp>
#include <com/sun/star/frame/XStatusListener.hpp>
#include <com/sun/star/frame/XStorable.hpp>
#include <com/sun/star/frame/status/FontHeight.hpp>
#include <com/sun/star/frame/theAutoRecovery.hpp>
#include <com/sun/star/frame/theUICommandDescription.hpp>
#include <com/sun/star/gallery/GalleryItemType.hpp>
#include <com/sun/star/geometry/RealPoint2D.hpp>
#include <com/sun/star/graphic/GraphicProvider.hpp>
#include <com/sun/star/graphic/GraphicType.hpp>
#include <com/sun/star/graphic/XGraphicProvider.hpp>
#include <com/sun/star/i18n/BreakIterator.hpp>
#include <com/sun/star/i18n/CharacterIteratorMode.hpp>
#include <com/sun/star/i18n/Collator.hpp>
#include <com/sun/star/i18n/ScriptType.hpp>
#include <com/sun/star/i18n/TextConversionOption.hpp>
#include <com/sun/star/i18n/TransliterationModules.hpp>
#include <com/sun/star/i18n/TransliterationModulesExtra.hpp>
#include <com/sun/star/i18n/WordType.hpp>
#include <com/sun/star/i18n/XForbiddenCharacters.hpp>
#include <com/sun/star/io/XActiveDataControl.hpp>
#include <com/sun/star/io/XActiveDataSource.hpp>
#include <com/sun/star/io/XInputStream.hpp>
#include <com/sun/star/io/XStream.hpp>
#include <com/sun/star/lang/DisposedException.hpp>
#include <com/sun/star/lang/IllegalAccessException.hpp>
#include <com/sun/star/lang/IllegalArgumentException.hpp>
#include <com/sun/star/lang/IndexOutOfBoundsException.hpp>
#include <com/sun/star/lang/Locale.hpp>
#include <com/sun/star/lang/NoSupportException.hpp>
#include <com/sun/star/lang/ServiceNotRegisteredException.hpp>
#include <com/sun/star/lang/XComponent.hpp>
#include <com/sun/star/lang/XInitialization.hpp>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>
#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/lang/XSingleServiceFactory.hpp>
#include <com/sun/star/lang/XUnoTunnel.hpp>
#include <com/sun/star/linguistic2/XHyphenator.hpp>
#include <com/sun/star/linguistic2/XSpellChecker1.hpp>
#include <com/sun/star/linguistic2/XThesaurus.hpp>
#include <com/sun/star/media/XManager.hpp>
#include <com/sun/star/media/XPlayer.hpp>
#include <com/sun/star/office/XAnnotation.hpp>
#include <com/sun/star/office/XAnnotationAccess.hpp>
#include <com/sun/star/office/XAnnotationEnumeration.hpp>
#include <com/sun/star/packages/zip/ZipIOException.hpp>
#include <com/sun/star/presentation/AnimationEffect.hpp>
#include <com/sun/star/presentation/AnimationSpeed.hpp>
#include <com/sun/star/presentation/ClickAction.hpp>
#include <com/sun/star/presentation/EffectCommands.hpp>
#include <com/sun/star/presentation/EffectNodeType.hpp>
#include <com/sun/star/presentation/EffectPresetClass.hpp>
#include <com/sun/star/presentation/FadeEffect.hpp>
#include <com/sun/star/presentation/ParagraphTarget.hpp>
#include <com/sun/star/presentation/PresentationRange.hpp>
#include <com/sun/star/presentation/ShapeAnimationSubType.hpp>
#include <com/sun/star/presentation/SlideShow.hpp>
#include <com/sun/star/presentation/TextAnimationType.hpp>
#include <com/sun/star/presentation/XPresentation2.hpp>
#include <com/sun/star/presentation/XSlideShowController.hpp>
#include <com/sun/star/registry/XRegistryKey.hpp>
#include <com/sun/star/rendering/XBitmapCanvas.hpp>
#include <com/sun/star/rendering/XSpriteCanvas.hpp>
#include <com/sun/star/scanner/ScannerManager.hpp>
#include <com/sun/star/sdbc/XResultSet.hpp>
#include <com/sun/star/sdbc/XRow.hpp>
#include <com/sun/star/style/XStyle.hpp>
#include <com/sun/star/style/XStyleFamiliesSupplier.hpp>
#include <com/sun/star/task/XInteractionHandler.hpp>
#include <com/sun/star/task/XInteractionRequest.hpp>
#include <com/sun/star/task/XStatusIndicatorFactory.hpp>
#include <com/sun/star/text/WritingMode.hpp>
#include <com/sun/star/text/XText.hpp>
#include <com/sun/star/text/XTextField.hpp>
#include <com/sun/star/text/XTextRange.hpp>
#include <com/sun/star/text/XTextRangeCompare.hpp>
#include <com/sun/star/ucb/SimpleFileAccess.hpp>
#include <com/sun/star/ucb/XCommandEnvironment.hpp>
#include <com/sun/star/ucb/XContentAccess.hpp>
#include <com/sun/star/ucb/XSimpleFileAccess2.hpp>
#include <com/sun/star/ui/UIElementType.hpp>
#include <com/sun/star/ui/dialogs/CommonFilePickerElementIds.hpp>
#include <com/sun/star/ui/dialogs/ExecutableDialogResults.hpp>
#include <com/sun/star/ui/dialogs/ExtendedFilePickerElementIds.hpp>
#include <com/sun/star/ui/dialogs/ListboxControlActions.hpp>
#include <com/sun/star/ui/dialogs/TemplateDescription.hpp>
#include <com/sun/star/ui/dialogs/XExecutableDialog.hpp>
#include <com/sun/star/ui/dialogs/XFilePicker.hpp>
#include <com/sun/star/ui/dialogs/XFilePickerControlAccess.hpp>
#include <com/sun/star/ui/dialogs/XFilePickerListener.hpp>
#include <com/sun/star/ui/dialogs/XFilePickerNotifier.hpp>
#include <com/sun/star/ui/dialogs/XFilterManager.hpp>
#include <com/sun/star/ui/dialogs/XSLTFilterDialog.hpp>
#include <com/sun/star/uno/Any.h>
#include <com/sun/star/uno/Any.hxx>
#include <com/sun/star/uno/Reference.hxx>
#include <com/sun/star/uno/RuntimeException.hpp>
#include <com/sun/star/uno/Sequence.h>
#include <com/sun/star/uno/Sequence.hxx>
#include <com/sun/star/uno/XComponentContext.hpp>
#include <com/sun/star/util/Color.hpp>
#include <com/sun/star/util/DateTime.hpp>
#include <com/sun/star/util/URL.hpp>
#include <com/sun/star/util/URLTransformer.hpp>
#include <com/sun/star/util/XChangesBatch.hpp>
#include <com/sun/star/util/XChangesNotifier.hpp>
#include <com/sun/star/util/XCloneable.hpp>
#include <com/sun/star/util/XCloseable.hpp>
#include <com/sun/star/util/XURLTransformer.hpp>
#include <com/sun/star/util/theMacroExpander.hpp>
#include <com/sun/star/view/DocumentZoomType.hpp>
#include <com/sun/star/view/PaperOrientation.hpp>
#include <com/sun/star/view/XSelectionSupplier.hpp>
#include <com/sun/star/xml/dom/DocumentBuilder.hpp>
#include <com/sun/star/xml/dom/XDocument.hpp>
#include <com/sun/star/xml/dom/XDocumentBuilder.hpp>
#include <com/sun/star/xml/dom/XNamedNodeMap.hpp>
#include <com/sun/star/xml/dom/XNode.hpp>
#include <com/sun/star/xml/dom/XNodeList.hpp>
#include <com/sun/star/xml/sax/InputSource.hpp>
#include <com/sun/star/xml/sax/Parser.hpp>
#include <com/sun/star/xml/sax/SAXParseException.hpp>
#include <com/sun/star/xml/sax/Writer.hpp>
#include <com/sun/star/xml/sax/XDTDHandler.hpp>
#include <com/sun/star/xml/sax/XDocumentHandler.hpp>
#include <com/sun/star/xml/sax/XEntityResolver.hpp>
#include <com/sun/star/xml/sax/XErrorHandler.hpp>
#include <comphelper/accessibleeventnotifier.hxx>
#include <comphelper/anytostring.hxx>
#include <comphelper/classids.hxx>
#include <comphelper/documentconstants.hxx>
#include <comphelper/expandmacro.hxx>
#include <comphelper/extract.hxx>
#include <comphelper/genericpropertyset.hxx>
#include <comphelper/namedvaluecollection.hxx>
#include <comphelper/oslfile2streamwrap.hxx>
#include <comphelper/processfactory.hxx>
#include <comphelper/propertysethelper.hxx>
#include <comphelper/propertysetinfo.hxx>
#include <comphelper/scopeguard.hxx>
#include <comphelper/sequence.hxx>
#include <comphelper/servicehelper.hxx>
#include <comphelper/serviceinfohelper.hxx>
#include <comphelper/storagehelper.hxx>
#include <comphelper/string.hxx>
#include <config_features.h>
#include <config_options.h>
#include <cppcanvas/basegfxfactory.hxx>
#include <cppcanvas/vclfactory.hxx>
#include <cppuhelper/basemutex.hxx>
#include <cppuhelper/bootstrap.hxx>
#include <cppuhelper/compbase1.hxx>
#include <cppuhelper/compbase2.hxx>
#include <cppuhelper/compbase4.hxx>
#include <cppuhelper/exc_hlp.hxx>
#include <cppuhelper/factory.hxx>
#include <cppuhelper/implbase1.hxx>
#include <cppuhelper/implbase2.hxx>
#include <cppuhelper/implbase3.hxx>
#include <cppuhelper/implbase5.hxx>
#include <cppuhelper/propertysetmixin.hxx>
#include <cppuhelper/proptypehlp.hxx>
#include <cppuhelper/supportsservice.hxx>
#include <cppuhelper/typeprovider.hxx>
#include <cppuhelper/weak.hxx>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <drawinglayer/geometry/viewinformation2d.hxx>
#include <drawinglayer/primitive2d/groupprimitive2d.hxx>
#include <drawinglayer/primitive2d/polygonprimitive2d.hxx>
#include <drawinglayer/primitive2d/structuretagprimitive2d.hxx>
#include <drawinglayer/primitive2d/textlayoutdevice.hxx>
#include <drawinglayer/primitive2d/textprimitive2d.hxx>
#include <editeng/UnoForbiddenCharsTable.hxx>
#include <editeng/adjustitem.hxx>
#include <editeng/autokernitem.hxx>
#include <editeng/borderline.hxx>
#include <editeng/boxitem.hxx>
#include <editeng/brushitem.hxx>
#include <editeng/bulletitem.hxx>
#include <editeng/charreliefitem.hxx>
#include <editeng/cmapitem.hxx>
#include <editeng/colritem.hxx>
#include <editeng/contouritem.hxx>
#include <editeng/crossedoutitem.hxx>
#include <editeng/editdata.hxx>
#include <editeng/editeng.hxx>
#include <editeng/editerr.hxx>
#include <editeng/editobj.hxx>
#include <editeng/editstat.hxx>
#include <editeng/editund2.hxx>
#include <editeng/editview.hxx>
#include <editeng/eeitem.hxx>
#include <editeng/emphasismarkitem.hxx>
#include <editeng/escapementitem.hxx>
#include <editeng/fhgtitem.hxx>
#include <editeng/flditem.hxx>
#include <editeng/flstitem.hxx>
#include <editeng/fontitem.hxx>
#include <editeng/forbiddencharacterstable.hxx>
#include <editeng/frmdir.hxx>
#include <editeng/frmdiritem.hxx>
#include <editeng/kernitem.hxx>
#include <editeng/langitem.hxx>
#include <editeng/lineitem.hxx>
#include <editeng/lrspitem.hxx>
#include <editeng/lspcitem.hxx>
#include <editeng/measfld.hxx>
#include <editeng/numdef.hxx>
#include <editeng/numitem.hxx>
#include <editeng/outliner.hxx>
#include <editeng/outlobj.hxx>
#include <editeng/paperinf.hxx>
#include <editeng/pbinitem.hxx>
#include <editeng/postitem.hxx>
#include <editeng/protitem.hxx>
#include <editeng/scriptspaceitem.hxx>
#include <editeng/scripttypeitem.hxx>
#include <editeng/shaditem.hxx>
#include <editeng/shdditem.hxx>
#include <editeng/sizeitem.hxx>
#include <editeng/svxenum.hxx>
#include <editeng/svxfont.hxx>
#include <editeng/tstpitem.hxx>
#include <editeng/udlnitem.hxx>
#include <editeng/ulspitem.hxx>
#include <editeng/unoedhlp.hxx>
#include <editeng/unofield.hxx>
#include <editeng/unolingu.hxx>
#include <editeng/unonrule.hxx>
#include <editeng/unotext.hxx>
#include <editeng/wghtitem.hxx>
#include <editeng/writingmodeitem.hxx>
#include <editeng/xmlcnitm.hxx>
#include <filter/msfilter/msdffimp.hxx>
#include <filter/msfilter/msoleexp.hxx>
#include <filter/msfilter/svxmsbas.hxx>
#include <i18nlangtag/languagetag.hxx>
#include <i18nlangtag/mslangid.hxx>
#include <i18nutil/unicode.hxx>
#include <iterator>
#include <limits.h>
#include <linguistic/lngprops.hxx>
#include <list>
#include <map>
#include <math.h>
#include <memory>
#include <new>
#include <numeric>
#include <o3tl/compat_functional.hxx>
#include <officecfg/Office/Common.hxx>
#include <officecfg/Office/Impress.hxx>
#include <osl/diagnose.h>
#include <osl/diagnose.hxx>
#include <osl/doublecheckedlocking.h>
#include <osl/file.hxx>
#include <osl/getglobalmutex.hxx>
#include <osl/module.hxx>
#include <osl/mutex.hxx>
#include <osl/thread.hxx>
#include <osl/time.h>
#include <rtl/instance.hxx>
#include <rtl/math.hxx>
#include <rtl/ref.hxx>
#include <rtl/strbuf.hxx>
#include <rtl/uri.hxx>
#include <rtl/ustrbuf.hxx>
#include <rtl/ustring.h>
#include <rtl/ustring.hxx>
#include <sal/config.h>
#include <sal/macros.h>
#include <sal/types.h>
#include <set>
#include <sfx2/app.hxx>
#include <sfx2/basedlgs.hxx>
#include <sfx2/bindings.hxx>
#include <sfx2/childwin.hxx>
#include <sfx2/ctrlitem.hxx>
#include <sfx2/dinfdlg.hxx>
#include <sfx2/dispatch.hxx>
#include <sfx2/docfac.hxx>
#include <sfx2/docfile.hxx>
#include <sfx2/docfilt.hxx>
#include <sfx2/dockwin.hxx>
#include <sfx2/doctempl.hxx>
#include <sfx2/event.hxx>
#include <sfx2/fcontnr.hxx>
#include <sfx2/filedlghelper.hxx>
#include <sfx2/frame.hxx>
#include <sfx2/imagemgr.hxx>
#include <sfx2/imgmgr.hxx>
#include <sfx2/infobar.hxx>
#include <sfx2/ipclient.hxx>
#include <sfx2/linkmgr.hxx>
#include <sfx2/mnumgr.hxx>
#include <sfx2/msg.hxx>
#include <sfx2/msgpool.hxx>
#include <sfx2/objface.hxx>
#include <sfx2/objsh.hxx>
#include <sfx2/opengrf.hxx>
#include <sfx2/printer.hxx>
#include <sfx2/progress.hxx>
#include <sfx2/request.hxx>
#include <sfx2/sfxdlg.hxx>
#include <sfx2/sfxmodelfactory.hxx>
#include <sfx2/sfxresid.hxx>
#include <sfx2/shell.hxx>
#include <sfx2/sidebar/EnumContext.hxx>
#include <sfx2/sidebar/Sidebar.hxx>
#include <sfx2/sidebar/SidebarChildWindow.hxx>
#include <sfx2/sidebar/SidebarPanelBase.hxx>
#include <sfx2/sidebar/Theme.hxx>
#include <sfx2/templdlg.hxx>
#include <sfx2/thumbnailview.hxx>
#include <sfx2/tplpitem.hxx>
#include <sfx2/viewfac.hxx>
#include <sfx2/viewfrm.hxx>
#include <sfx2/viewsh.hxx>
#include <sfx2/zoomitem.hxx>
#include <sot/exchange.hxx>
#include <sot/filelist.hxx>
#include <sot/formats.hxx>
#include <sot/object.hxx>
#include <sot/storage.hxx>
#include <stdio.h>
#include <string.h>
#include <string>
#include <svl/IndexedStyleSheets.hxx>
#include <svl/aeitem.hxx>
#include <svl/SfxBroadcaster.hxx>
#include <svl/cjkoptions.hxx>
#include <svl/ctloptions.hxx>
#include <svl/eitem.hxx>
#include <svl/flagitem.hxx>
#include <svl/globalnameitem.hxx>
#include <svl/inethist.hxx>
#include <svl/instrm.hxx>
#include <svl/intitem.hxx>
#include <svl/itemiter.hxx>
#include <svl/itempool.hxx>
#include <svl/itemprop.hxx>
#include <svl/itemset.hxx>
#include <svl/languageoptions.hxx>
#include <svl/lckbitem.hxx>
#include <svl/lstner.hxx>
#include <svl/poolitem.hxx>
#include <svl/ptitem.hxx>
#include <svl/rectitem.hxx>
#include <svl/slstitm.hxx>
#include <svl/smplhint.hxx>
#include <svl/srchitem.hxx>
#include <svl/stritem.hxx>
#include <svl/style.hxx>
#include <svl/urihelper.hxx>
#include <svl/urlbmk.hxx>
#include <svl/visitem.hxx>
#include <svl/whiter.hxx>
#include <svl/zforlist.hxx>
#include <svtools/cliplistener.hxx>
#include <svtools/colorcfg.hxx>
#include <svtools/ctrlbox.hxx>
#include <svtools/ctrltool.hxx>
#include <svtools/ehdl.hxx>
#include <svtools/embedtransfer.hxx>
#include <svtools/htmlout.hxx>
#include <svtools/imapcirc.hxx>
#include <svtools/imapobj.hxx>
#include <svtools/imappoly.hxx>
#include <svtools/imaprect.hxx>
#include <svtools/insdlg.hxx>
#include <svtools/langtab.hxx>
#include <svtools/miscopt.hxx>
#include <svtools/sfxecode.hxx>
#include <svtools/soerr.hxx>
#include <svtools/svlbitm.hxx>
#include <svtools/svmedit.hxx>
#include <svtools/svtresid.hxx>
#include <svtools/tabbar.hxx>
#include <svtools/toolbarmenu.hxx>
#include <svtools/transfer.hxx>
#include <svtools/treelistentry.hxx>
#include <svtools/unoevent.hxx>
#include <svtools/unoimap.hxx>
#include <svtools/valueset.hxx>
#include <svx/AccessibleShape.hxx>
#include <svx/AccessibleShapeInfo.hxx>
#include <svx/AffineMatrixItem.hxx>
#include <svx/DescriptionGenerator.hxx>
#include <svx/ShapeTypeHandler.hxx>
#include <svx/SpellDialogChildWindow.hxx>
#include <svx/SvxColorChildWindow.hxx>
#include <svx/SvxShapeTypes.hxx>
#include <svx/UnoNamespaceMap.hxx>
#include <svx/algitem.hxx>
#include <svx/bmpmask.hxx>
#include <svx/camera3d.hxx>
#include <svx/charthelper.hxx>
#include <svx/chrtitem.hxx>
#include <svx/clipboardctl.hxx>
#include <svx/clipfmtitem.hxx>
#include <svx/colrctrl.hxx>
#include <svx/compressgraphicdialog.hxx>
#include <svx/cube3d.hxx>
#include <svx/dataaccessdescriptor.hxx>
#include <svx/dialmgr.hxx>
#include <svx/dlgutil.hxx>
#include <svx/drawitem.hxx>
#include <svx/e3dundo.hxx>
#include <svx/extedit.hxx>
#include <svx/extrusionbar.hxx>
#include <svx/f3dchild.hxx>
#include <svx/fillctrl.hxx>
#include <svx/float3d.hxx>
#include <svx/fmglob.hxx>
#include <svx/fmmodel.hxx>
#include <svx/fmobjfac.hxx>
#include <svx/fmshell.hxx>
#include <svx/fmview.hxx>
#include <svx/fntctl.hxx>
#include <svx/fntszctl.hxx>
#include <svx/fontwork.hxx>
#include <svx/fontworkbar.hxx>
#include <svx/fontworkgallery.hxx>
#include <svx/formatpaintbrushctrl.hxx>
#include <svx/galbrws.hxx>
#include <svx/gallery.hxx>
#include <svx/galleryitem.hxx>
#include <svx/globl3d.hxx>
#include <svx/grafctrl.hxx>
#include <svx/graphichelper.hxx>
#include <svx/grfflt.hxx>
#include <svx/hlnkitem.hxx>
#include <svx/hyperdlg.hxx>
#include <svx/imapdlg.hxx>
#include <svx/lathe3d.hxx>
#include <svx/layctrl.hxx>
#include <svx/lboxctrl.hxx>
#include <svx/linectrl.hxx>
#include <svx/linkwarn.hxx>
#include <svx/modctrl.hxx>
#include <svx/nbdtmg.hxx>
#include <svx/nbdtmgfact.hxx>
#include <svx/obj3d.hxx>
#include <svx/objfac3d.hxx>
#include <svx/ofaitem.hxx>
#include <svx/pageitem.hxx>
#include <svx/pfiledlg.hxx>
#include <svx/polypolygoneditor.hxx>
#include <svx/polysc3d.hxx>
#include <svx/postattr.hxx>
#include <svx/prtqry.hxx>
#include <svx/pszctrl.hxx>
#include <svx/ruler.hxx>
#include <svx/rulritem.hxx>
#include <svx/scene3d.hxx>
#include <svx/sdasitm.hxx>
#include <svx/sderitm.hxx>
#include <svx/sdr/contact/displayinfo.hxx>
#include <svx/sdr/contact/objectcontact.hxx>
#include <svx/sdr/contact/viewcontact.hxx>
#include <svx/sdr/contact/viewcontactofsdrmediaobj.hxx>
#include <svx/sdr/contact/viewobjectcontact.hxx>
#include <svx/sdr/overlay/overlayanimatedbitmapex.hxx>
#include <svx/sdr/overlay/overlaybitmapex.hxx>
#include <svx/sdr/overlay/overlaymanager.hxx>
#include <svx/sdr/overlay/overlayobjectcell.hxx>
#include <svx/sdr/overlay/overlaypolypolygon.hxx>
#include <svx/sdr/overlay/overlayprimitive2dsequenceobject.hxx>
#include <svx/sdr/properties/attributeproperties.hxx>
#include <svx/sdr/properties/properties.hxx>
#include <svx/sdr/table/tablecontroller.hxx>
#include <svx/sdr/table/tabledesign.hxx>
#include <svx/sdrhittesthelper.hxx>
#include <svx/sdrpagewindow.hxx>
#include <svx/sdrpaintwindow.hxx>
#include <svx/sdshcitm.hxx>
#include <svx/sdshitm.hxx>
#include <svx/sdtagitm.hxx>
#include <svx/sdtmfitm.hxx>
#include <svx/sidebar/ContextChangeEventMultiplexer.hxx>
#include <svx/sidebar/SelectionAnalyzer.hxx>
#include <svx/sphere3d.hxx>
#include <svx/srchdlg.hxx>
#include <svx/subtoolboxcontrol.hxx>
#include <svx/svdattr.hxx>
#include <svx/svddef.hxx>
#include <svx/svddrgmt.hxx>
#include <svx/svdedtv.hxx>
#include <svx/svdetc.hxx>
#include <svx/svdfield.hxx>
#include <svx/svdglue.hxx>
#include <svx/svditer.hxx>
#include <svx/svdlayer.hxx>
#include <svx/svdmodel.hxx>
#include <svx/svdoashp.hxx>
#include <svx/svdoattr.hxx>
#include <svx/svdobj.hxx>
#include <svx/svdocapt.hxx>
#include <svx/svdocirc.hxx>
#include <svx/svdograf.hxx>
#include <svx/svdogrp.hxx>
#include <svx/svdomeas.hxx>
#include <svx/svdomedia.hxx>
#include <svx/svdoole2.hxx>
#include <svx/svdopage.hxx>
#include <svx/svdopath.hxx>
#include <svx/svdorect.hxx>
#include <svx/svdotable.hxx>
#include <svx/svdotext.hxx>
#include <svx/svdouno.hxx>
#include <svx/svdoutl.hxx>
#include <svx/svdpage.hxx>
#include <svx/svdpagv.hxx>
#include <svx/svdpntv.hxx>
#include <svx/svdpool.hxx>
#include <svx/svdsob.hxx>
#include <svx/svdtypes.hxx>
#include <svx/svdundo.hxx>
#include <svx/svdview.hxx>
#include <svx/svdviter.hxx>
#include <svx/svx3ditems.hxx>
#include <svx/svxdlg.hxx>
#include <svx/svxerr.hxx>
#include <svx/sxciaitm.hxx>
#include <svx/sxekitm.hxx>
#include <svx/sxelditm.hxx>
#include <svx/sxmsuitm.hxx>
#include <svx/tabline.hxx>
#include <svx/tbcontrl.hxx>
#include <svx/tbxcustomshapes.hxx>
#include <svx/unoapi.hxx>
#include <svx/unofill.hxx>
#include <svx/unomodel.hxx>
#include <svx/unopage.hxx>
#include <svx/unopool.hxx>
#include <svx/unoprov.hxx>
#include <svx/unoshape.hxx>
#include <svx/unoshprp.hxx>
#include <svx/verttexttbxctrl.hxx>
#include <svx/view3d.hxx>
#include <svx/xbtmpit.hxx>
#include <svx/xcolit.hxx>
#include <svx/xdef.hxx>
#include <svx/xenum.hxx>
#include <svx/xexch.hxx>
#include <svx/xfillit.hxx>
#include <svx/xfillit0.hxx>
#include <svx/xflbmtit.hxx>
#include <svx/xflbstit.hxx>
#include <svx/xflclit.hxx>
#include <svx/xflftrit.hxx>
#include <svx/xflgrit.hxx>
#include <svx/xflhtit.hxx>
#include <svx/xftadit.hxx>
#include <svx/xftdiit.hxx>
#include <svx/xftmrit.hxx>
#include <svx/xftouit.hxx>
#include <svx/xftshcit.hxx>
#include <svx/xftshit.hxx>
#include <svx/xftshxy.hxx>
#include <svx/xftstit.hxx>
#include <svx/xgrad.hxx>
#include <svx/xit.hxx>
#include <svx/xlineit.hxx>
#include <svx/xlineit0.hxx>
#include <svx/xlinjoit.hxx>
#include <svx/xlncapit.hxx>
#include <svx/xlnclit.hxx>
#include <svx/xlndsit.hxx>
#include <svx/xlnedcit.hxx>
#include <svx/xlnedit.hxx>
#include <svx/xlnedwit.hxx>
#include <svx/xlnstcit.hxx>
#include <svx/xlnstit.hxx>
#include <svx/xlnstwit.hxx>
#include <svx/xlntrit.hxx>
#include <svx/xlnwtit.hxx>
#include <svx/xmleohlp.hxx>
#include <svx/xmlgrhlp.hxx>
#include <svx/xmlsecctrl.hxx>
#include <svx/xoutbmp.hxx>
#include <svx/xsetit.hxx>
#include <svx/xtable.hxx>
#include <svx/xtextit0.hxx>
#include <svx/zoom_def.hxx>
#include <svx/zoomctrl.hxx>
#include <svx/zoomsliderctrl.hxx>
#include <svx/zoomslideritem.hxx>
#include <time.h>
#include <toolkit/awt/vclxdevice.hxx>
#include <toolkit/helper/vclunohelper.hxx>
#include <tools/color.hxx>
#include <tools/datetime.hxx>
#include <tools/debug.hxx>
#include <tools/diagnose_ex.h>
#include <tools/errinf.hxx>
#include <tools/gen.hxx>
#include <tools/globname.hxx>
#include <tools/helpers.hxx>
#include <tools/link.hxx>
#include <tools/multisel.hxx>
#include <tools/poly.hxx>
#include <tools/rcid.h>
#include <tools/resary.hxx>
#include <tools/resmgr.hxx>
#include <tools/shl.hxx>
#include <tools/stream.hxx>
#include <tools/tenccvt.hxx>
#include <tools/time.hxx>
#include <tools/urlobj.hxx>
#include <tools/wintypes.hxx>
#include <tools/wldcrd.hxx>
#include <unotools/accessiblestatesethelper.hxx>
#include <unotools/charclass.hxx>
#include <unotools/confignode.hxx>
#include <unotools/fltrcfg.hxx>
#include <unotools/lingucfg.hxx>
#include <unotools/linguprops.hxx>
#include <unotools/localedatawrapper.hxx>
#include <unotools/localfilehelper.hxx>
#include <unotools/mediadescriptor.hxx>
#include <unotools/moduleoptions.hxx>
#include <unotools/pathoptions.hxx>
#include <unotools/saveopt.hxx>
#include <unotools/securityoptions.hxx>
#include <unotools/streamwrap.hxx>
#include <unotools/syslocale.hxx>
#include <unotools/tempfile.hxx>
#include <unotools/ucbstreamhelper.hxx>
#include <unotools/useroptions.hxx>
#include <unotools/viewoptions.hxx>
#include <utility>
#include <vcl/FilterConfigItem.hxx>
#include <vcl/abstdlg.hxx>
#include <vcl/bitmap.hxx>
#include <vcl/bitmapex.hxx>
#include <vcl/bmpacc.hxx>
#include <vcl/builder.hxx>
#include <vcl/button.hxx>
#include <vcl/combobox.hxx>
#include <vcl/ctrl.hxx>
#include <vcl/cursor.hxx>
#include <vcl/cvtgrf.hxx>
#include <vcl/decoview.hxx>
#include <vcl/dialog.hxx>
#include <vcl/dibtools.hxx>
#include <vcl/edit.hxx>
#include <vcl/field.hxx>
#include <vcl/fixed.hxx>
#include <vcl/floatwin.hxx>
#include <vcl/font.hxx>
#include <vcl/gdimtf.hxx>
#include <vcl/gradient.hxx>
#include <vcl/graph.hxx>
#include <vcl/graphicfilter.hxx>
#include <vcl/group.hxx>
#include <vcl/help.hxx>
#include <vcl/image.hxx>
#include <vcl/layout.hxx>
#include <vcl/lazydelete.hxx>
#include <vcl/lineinfo.hxx>
#include <vcl/lstbox.hxx>
#include <vcl/menu.hxx>
#include <vcl/menubtn.hxx>
#include <vcl/metaact.hxx>
#include <vcl/metric.hxx>
#include <vcl/msgbox.hxx>
#include <vcl/outdev.hxx>
#include <vcl/pdfextoutdevdata.hxx>
#include <vcl/pngread.hxx>
#include <vcl/pngwrite.hxx>
#include <vcl/pointr.hxx>
#include <vcl/prntypes.hxx>
#include <vcl/scrbar.hxx>
#include <vcl/seleng.hxx>
#include <vcl/settings.hxx>
#include <vcl/split.hxx>
#include <vcl/splitwin.hxx>
#include <vcl/status.hxx>
#include <vcl/svapp.hxx>
#include <vcl/tabctrl.hxx>
#include <vcl/tabpage.hxx>
#include <vcl/taskpanelist.hxx>
#include <vcl/toolbox.hxx>
#include <vcl/vclenum.hxx>
#include <vcl/virdev.hxx>
#include <vcl/waitobj.hxx>
#include <vcl/window.hxx>
#include <vcl/wmf.hxx>
#include <vcl/wrkwin.hxx>
#include <vector>
#include <xmloff/settingsstore.hxx>

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */

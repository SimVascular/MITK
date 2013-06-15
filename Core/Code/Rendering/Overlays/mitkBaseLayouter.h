/*===================================================================
 *
The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center,
Division of Medical and Biological Informatics.
All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt or http://www.mitk.org for details.

===================================================================*/

#ifndef BASELAYOUTER_H
#define BASELAYOUTER_H

#include <MitkExports.h>
#include <itkObject.h>
#include <mitkCommon.h>
#include "mitkBaseRenderer.h"
#include "mitkOverlay.h"

namespace mitk {

class MITK_CORE_EXPORT BaseLayouter : public itk::Object {
public:

  mitkClassMacro(BaseLayouter, itk::Object);

  void SetBaseRenderer(BaseRenderer::Pointer renderer);
  BaseRenderer::Pointer GetBaseRenderer();

  void AddOverlay(mitk::Overlay::Pointer Overlay);
  void RemoveOverlay(mitk::Overlay::Pointer Overlay);
  std::string GetIdentifier();
  virtual void PrepareLayout() = 0;


protected:

  /** \brief explicit constructor which disallows implicit conversions */
  explicit BaseLayouter();

  /** \brief virtual destructor in order to derive from this class */
  virtual ~BaseLayouter();

  std::list<mitk::Overlay::Pointer> GetManagedOverlays();
  std::string m_Identifier;

private:

  mitk::BaseRenderer::Pointer m_BaseRenderer;
  std::list<mitk::Overlay*> m_ManagedOverlays;

  /** \brief copy constructor */
  BaseLayouter( const BaseLayouter &);

  /** \brief assignment operator */
  BaseLayouter &operator=(const BaseLayouter &);
};

} // namespace mitk
#endif // BASELAYOUTER_H



#ifndef __itkStrainImageFilter_hxx
#define __itkStrainImageFilter_hxx

#include "itkStrainImageFilter.h"

#include "itkGradientImageFilter.h"
#include "itkImageRegionConstIterator.h"
#include "itkImageRegionIterator.h"

namespace itk
{

template < class TInputImage, class TOperatorValueType,
           class TOutputValueType >
StrainImageFilter< TInputImage, TOperatorValueType, TOutputValueType >
::StrainImageFilter():
  m_StrainForm( INFINITESIMAL )
{
  // The first output is only of interest to the user.  The rest of the outputs
  // are GradientImageFilter outputs used internally, but put on the output so
  // memory management capabilities of the pipeline can be taken advantage of.
  this->SetNumberOfIndexedOutputs( 1 + ImageDimension );
  for( unsigned int i = 1; i < ImageDimension + 1; i++ )
    {
    this->SetNthOutput( i, GradientOutputImageType::New().GetPointer() );
    }

  this->m_InputComponentsFilter = InputComponentsImageFilterType::New();

  typedef itk::GradientImageFilter< OperatorImageType, TOperatorValueType, TOperatorValueType >
    GradientImageFilterType;
  this->m_GradientFilter   = GradientImageFilterType::New().GetPointer();

  this->m_VectorGradientFilter = NULL;
}

template < class TInputImage, class TOperatorValueType,
           class TOutputValueType >
void
StrainImageFilter< TInputImage, TOperatorValueType, TOutputValueType >
::BeforeThreadedGenerateData()
{
  typename InputImageType::ConstPointer input = this->GetInput();

  if( this->m_VectorGradientFilter.GetPointer() != NULL )
    {
    this->m_VectorGradientFilter->SetInput( input );
    for( unsigned int i = 1; i < ImageDimension + 1; ++i )
      {
      this->m_VectorGradientFilter->GraftNthOutput( i - 1, dynamic_cast< GradientOutputImageType* >(
          this->ProcessObject::GetOutput( i ) ) );
      }
    this->m_VectorGradientFilter->Update();
    for( unsigned int i = 1; i < ImageDimension + 1; ++i )
      {
      this->GraftNthOutput( i, this->m_VectorGradientFilter->GetOutput( i - 1 ) );
      }
    }
  else
    {
    this->m_InputComponentsFilter->SetInput( input );

    for( unsigned int i = 1; i < ImageDimension + 1; ++i )
      {
      this->m_GradientFilter->SetInput(
        this->m_InputComponentsFilter->GetOutput( i - 1 ) );
      this->m_GradientFilter->GraftOutput( dynamic_cast< GradientOutputImageType* >(
          this->ProcessObject::GetOutput( i ) ) );
      this->m_GradientFilter->Update();
      this->GraftNthOutput( i, this->m_GradientFilter->GetOutput() );
      }
    }
}


template < class TInputImage, class TOperatorValueType,
           class TOutputValueType >
void
StrainImageFilter< TInputImage, TOperatorValueType, TOutputValueType >
::ThreadedGenerateData( const OutputRegionType& region,
                        ThreadIdType itkNotUsed( threadId ) )
{
  typename InputImageType::ConstPointer input = this->GetInput();

  OutputImageType * output = this->GetOutput();

  ImageRegionIterator< OutputImageType > outputIt( output, region );
  // First fill the outputs with zeros.  Better way to do this?  FillBuffer does
  // not seem to work.
  typename OutputImageType::PixelType outputPixel;
  outputPixel.Fill( itk::NumericTraits< TOutputValueType >::Zero );
  // @todo use .Value() here?
  for( outputIt.GoToBegin(); !outputIt.IsAtEnd(); ++outputIt )
    outputIt.Set( outputPixel );
  unsigned int            j;
  unsigned int            k;
  GradientOutputPixelType gradientPixel;

  // e_ij += 1/2( du_i/dx_j + du_j/dx_i )
  for( unsigned int i = 0; i < ImageDimension; ++i )
    {
    itk::ImageRegionConstIterator< GradientOutputImageType >
      gradientIt( reinterpret_cast< GradientOutputImageType* >(
          dynamic_cast< GradientOutputImageType* >(
            this->ProcessObject::GetOutput( i + 1 ) ) )
        , region );
    for( outputIt.GoToBegin(), gradientIt.GoToBegin();
         !gradientIt.IsAtEnd();
         ++outputIt, ++gradientIt )
      {
      outputPixel = outputIt.Get();
      gradientPixel = gradientIt.Get();
      for( j = 0; j < i; ++j )
        {
        // @todo use .Value() here?
        outputPixel( i, j ) += gradientPixel[j] / static_cast< TOutputValueType >( 2 );
        }
      // j == i
      outputPixel( i, i ) += gradientPixel[i];
      for( j = i + 1; j < ImageDimension; ++j )
        {
        outputPixel( i, j ) += gradientPixel[j] / static_cast< TOutputValueType >( 2 );
        }
      outputIt.Set( outputPixel );
      }
    }
  switch( m_StrainForm )
    {
  case INFINITESIMAL:
      break;
  // e_ij += 1/2 du_m/du_i du_m/du_j
  case GREENLAGRANGIAN:
    for( unsigned int i = 0; i < ImageDimension; ++i )
      {
      itk::ImageRegionConstIterator< GradientOutputImageType >
        gradientIt( reinterpret_cast< GradientOutputImageType* >(
            dynamic_cast< GradientOutputImageType* >(
              this->ProcessObject::GetOutput( i + 1 ) ) )
          , region );
      for( outputIt.GoToBegin(), gradientIt.GoToBegin();
           !gradientIt.IsAtEnd();
           ++outputIt, ++gradientIt )
        {
        outputPixel = outputIt.Get();
        gradientPixel = gradientIt.Get();
        for( j = 0; j < ImageDimension; ++j )
          {
          for( k = 0; k <= j; ++k )
            {
            // @todo use .Value() here?
            outputPixel( j, k ) += gradientPixel[j] * gradientPixel[k] / static_cast< TOutputValueType >( 2 );
            }
          }
        outputIt.Set( outputPixel );
        }
      }
      break;
  // e_ij -= 1/2 du_m/du_i du_m/du_j
  case EULERIANALMANSI:
    for( unsigned int i = 0; i < ImageDimension; ++i )
      {
      itk::ImageRegionConstIterator< GradientOutputImageType >
        gradientIt( reinterpret_cast< GradientOutputImageType* >(
            dynamic_cast< GradientOutputImageType* >(
              this->ProcessObject::GetOutput( i + 1 ) ) )
          , region );
      for( outputIt.GoToBegin(), gradientIt.GoToBegin();
           !gradientIt.IsAtEnd();
           ++outputIt, ++gradientIt )
        {
        outputPixel = outputIt.Get();
        gradientPixel = gradientIt.Get();
        for( j = 0; j < ImageDimension; ++j )
          {
          for( k = 0; k <= j; ++k )
            {
            // @todo use .Value() here?
            outputPixel( j, k ) -= gradientPixel[j] * gradientPixel[k] / static_cast< TOutputValueType >( 2 );
            }
          }
        outputIt.Set( outputPixel );
        }
      }
      break;
  default:
    itkExceptionMacro( << "Unknown strain form." );
    }
}

} // end namespace itk

#endif